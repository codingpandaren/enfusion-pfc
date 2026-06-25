# Networking & MP

PFC is built for dedicated servers - everything is networked. The model follows Reforger's
ownership-transfer model: one peer is authoritative, the rest are interpolated proxies.

## Authority

- The **pilot's client** owns the vehicle and is authoritative; when the aircraft is unmanned the **server**
  owns it.
- `PFC_FlightModel.EOnSimulate` returns immediately unless the peer is the owner or the server, so the
  aerodynamic integration runs in exactly one place.
- `OnTicksOnRemoteProxy()` returns `true` so the component still ticks on proxies - needed for local visual
  updates (propeller spin) and to read replicated signals.

## Input relay

`PFC_FlightController` smooths input on the owning client, then relays it to the server:

```text
owner client ── Rpc(RpcSrv_ReceiveInputs, pitch, roll, yaw, throttle) ──► server
```

- Sent on an interval (`m_fInputSendIntervalMs`, default 16 ms) over an **unreliable** channel - it's a
  constantly-refreshed stream, so a dropped packet doesn't matter.
- A non-owner client never overwrites the pilot's stick: `PollInput` returns early for non-owners, which read
  the cached relayed values instead.

## Movement replication

`PFC_NwkMovementComponent` replicates the authoritative transform + velocity to proxies so remote viewers see
smooth motion. Proxies don't simulate aero; they interpolate this state and drive their own propeller visuals
from the replicated, normalized engine-RPM signal.

## Instrument signals

The owner publishes gauge signals via `SignalsManagerComponent`; they replicate to all peers, so cockpit
instruments and HUDs on every client read them for free.

!!! danger "Use the right compression function"
    `AddOrFindMPSignal(..., SignalCompressionFunc.X)` - the compression must match the value range:

    - **Raw units** (airspeed km/h, altitude m, climb m/s, pitch/bank/heading deg, G-load): use `None`.
      `Range01` would clamp them to `[0,1]` over the wire, pinning every receiver's gauge at `1`.
    - **Normalized 0..1** (throttle, normalized RPM): `Range01` is fine.
    - **Angles** (control-surface deflection): `RotDEG`.

## Boot-window guard

Writing an MP signal - or firing an RPC - on an entity that isn't yet **registered for replication** spams
`"unregistered item"` errors every frame during the spawn window. PFC guards both:

- Signal publishing is gated on `m_RplComponent.Id().IsValid()`.
- The input RPC is sent only when `!Replication.IsServer()` **and** the Rpl id is valid - if the server owns
  the plane (unmanned), there's no one to send to and it already holds the values locally.

!!! tip "Reuse these patterns in variants"
    Any per-tick RPC or MP-signal write you add in a variant should follow the same two guards
    (owner/server check + valid-Rpl-id check), or you'll reintroduce the boot-window log spam.
