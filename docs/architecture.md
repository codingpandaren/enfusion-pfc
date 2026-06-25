# Architecture

PFC is a custom flight model layered on top of Enfusion's rigid-body simulation. The engine integrates the
rigid body; PFC's scripts apply per-surface lift/drag and thrust via `Physics.ApplyImpulseAt`, plus
angular-rate damping to keep the airframe stable.

## Built on `Wheeled_Base.et`

The reference Cessna inherits the vanilla `Wheeled_Base.et`. The wheeled simulation is **neutralized** - only
the suspension and wheel-bone visuals remain - so the inherited `SCR_CarControllerComponent` still handles
ownership transfer and grants the input context (`CarContext`) that the flight controller reads.

!!! warning "Flying on a car base has sharp edges"
    The vanilla car controls share keys with flight: the handbrake stays engaged because `CarThrust` is 0,
    `CarBrake` is on ++s++ (= pitch back), and `CarShift` is on ++q++/++e++ (= yaw). PFC neutralizes all of
    these every frame. See [Prefab Setup](prefab-setup.md#neutralizing-the-wheeled-sim) for the full list of
    wheeled-sim settings that must be right, and [Input & Controls](input-and-controls.md) for the key
    de-conflicting.

## Components

| Component | Responsibility |
|---|---|
| `PFC_FlightModel` | The flight model. Builds aero surfaces, applies per-surface lift/drag + thrust impulses, spools engine RPM, angular damping, wind/gusts, publishes instrument signals, debug draw. Runs on owner + server. |
| `PFC_FlightController` | Reads pilot input from `CarContext`, smooths it (slew-rate limited), exposes pitch/roll/yaw/throttle to the flight model, relays input to the server, and injects ground steering. |
| `PFC_NwkMovementComponent` | Replicates owner→proxy movement state (transform + velocity) so remote viewers see smooth motion. |
| `PFC_AeroSurface` | A single aerodynamic panel: given local airflow + air density, returns the lift+drag force. Pure math, no engine state. |
| `PFC_AeroSurfaceConfig` | Runtime aero parameters for one surface (chord, span, lift slope, stall, etc.). |
| `PFC_AeroSurfaceDef` | The **editable** surface definition placed in the prefab; builds a config and resolves its axes, handles X-mirroring. |
| `PFC_PilotUtil` | Helper to determine the local *active* pilot. |

## Execution flow (per simulation tick)

```text
EOnSimulate (owner / server only)
 ├─ PFC_FlightController.PollInput()      → smoothed pitch/roll/yaw/throttle
 ├─ set each surface's flap deflection from its control axis
 ├─ guard against NaN / teleport-spike velocity (reset or clamp)
 ├─ RefreshWind()                          → steady wind + gradient + gusts
 ├─ for each surface: CalculateForce() → ApplyImpulseAt(surfacePos, force·dt)
 ├─ fuselage drag impulse
 ├─ spool engine RPM, apply thrust impulse at the centre of mass
 ├─ speed-scaled angular-rate damping
 ├─ compute normal G-load
 └─ (owner, once registered) UpdateSignals() → instrument MP signals

EOnFrame (all peers)
 ├─ PFC_FlightController.ApplyGroundSteering()  (also zeroes CarThrust/CarBrake/CarShift)
 ├─ UpdatePropellerVisuals()   (owner/server from local RPM; proxies from replicated RPM signal)
 └─ DrawDebug()                (F6)
```

## Authority model

- `EOnSimulate` returns immediately on a peer that is **neither the owner nor the server**, so the aerodynamic
  simulation only ever runs on the authoritative peer.
- The **owner** publishes instrument signals; they replicate to everyone for free.
- **Proxies** receive replicated movement via `PFC_NwkMovementComponent` and drive their own propeller spin
  from the replicated, normalized engine-RPM signal.
- `OnTicksOnRemoteProxy()` returns `true` so the component keeps ticking on proxies (needed for the local
  visual updates).

See [Networking & MP](networking.md) for the replication and signal details, including the compression and
boot-window pitfalls.
