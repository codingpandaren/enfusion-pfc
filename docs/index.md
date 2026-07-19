# Propeller Flight Core

**Propeller Flight Core (PFC)** is a lightweight, multiplayer-ready propeller-aircraft flight model for
Arma Reforger. It provides a custom per-surface aerodynamic simulation layered on top of Enfusion's rigid
body, packaged as a minimal, reusable core that other aircraft mods can build on.

The reference airframe shipped with the core is a **Cessna 172** built on the vanilla `Wheeled_Base.et`.

!!! info "Scope"
    PFC is deliberately *minimal*. It is the flight core only - per-surface lift/drag, thrust, angular
    damping, wind, instrument signals, and the networking to make all of that work on a dedicated server.
    It intentionally has **no** gear/flap/trim controllers, cargo, weapons, or cockpit systems. Those belong
    in a variant built on top of the core (see [Building a Variant](building-a-variant.md)).

## What it does

- **Per-surface aerodynamics** - each wing/tail panel computes its own lift and drag (Khan & Nahon 2015
  linear-to-flat-plate model) and applies it as a rigid-body impulse at the surface position.
- **Always-on engine** - RPM spools toward `idle + throttle·(max−idle)`; thrust scales from 0 at idle to
  full at max RPM. No start sequence (that is variant territory).
- **Wind & gusts** - airspeed is computed relative to the air mass, so crosswind produces sideslip /
  weathervaning and a head/tailwind splits IAS from groundspeed. A near-ground gradient and smooth gusts
  add buffeting.
- **Instrument signals** - publishes the standard vanilla gauge signals (airspeed, altitude, AGL, climb
  rate, RPM, pitch/bank/heading, G-load, control-surface angles) for HUDs and cockpit instruments.
- **MP / dedicated-server ready** - the owner client (or the server when unmanned) is authoritative; remote
  proxies receive replicated transform + velocity and locally drive their visuals.
- **Extensible by subclass** - protected variant hooks (spool, thrust, drag, surface deflection, per-surface
  airflow, end-of-tick) let a variant swap individual pieces of the sim without forking it. The
  **Jet Flight Core** mod builds its jet flight model this way (see
  [Building a Variant](building-a-variant.md#extending-the-flight-model-itself)).

## Key facts

| | |
|---|---|
| Mod GUID | `69A8A34027DA65C5` |
| Script class prefix | `PFC_` |
| Reference prefab | `Prefabs/Vehicles/Cessna172.et` (inherits `Wheeled_Base.et`) |
| Test world | `Worlds/MP_Test.ent` |
| Dependency | Arma Reforger core (`58D0FB3206B6F859`) |

## Where to go next

- New to the code? Start with [Getting Started](getting-started.md) then [Architecture](architecture.md).
- Tuning the sim? See [Flight Model](flight-model.md) and the [Reference](reference.md) tables.
- Wiring controls? See [Input & Controls](input-and-controls.md) - note the **input-config GUID pitfall**.
- Making your own aircraft? See [Building a Variant](building-a-variant.md).
