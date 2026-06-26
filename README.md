# Propeller Flight Core

**Propeller Flight Core (PFC)** is a lightweight, multiplayer-ready propeller-aircraft flight model for
**Arma Reforger**. It layers a custom per-surface aerodynamic simulation on top of Enfusion's rigid body and
ships as a minimal, reusable core that other aircraft mods can build on.

The reference airframe shipped with the core is a **Cessna 172** built on the vanilla `Wheeled_Base.et`.

## What it does

- **Per-surface aerodynamics** - each wing/tail panel computes its own lift and drag (Khan & Nahon 2015
  linear-to-flat-plate model) and applies it as a rigid-body impulse at the surface position.
- **Always-on engine** - RPM spools toward `idle + throttle·(max−idle)`; thrust scales from zero at idle to
  full at max RPM, and falls off with hull damage. No start sequence (that is variant territory).
- **Wind & gusts** - airspeed is computed relative to the air mass, so crosswind produces sideslip /
  weathervaning and a head/tailwind splits indicated airspeed from groundspeed. A near-ground wind gradient
  and smooth gusts add buffeting.
- **Instrument signals** - publishes the standard vanilla gauge signals (airspeed, altitude, AGL, climb
  rate, RPM, pitch/bank/heading, G-load, control-surface and propeller angles) for HUDs and cockpit
  instruments.
- **MP / dedicated-server ready** - the owner client (or the server when unmanned) is authoritative; remote
  proxies receive replicated transform + velocity and locally drive their visuals.
- **In-game debug tooling** - `F6 → Prop Flight` toggles force/lift/wind vector overlays and a live readout
  of AoA, pitch, flight-path angle, IAS, vertical speed, G-loads and wind.

!!! note "Scope"
    PFC is deliberately *minimal*. It is the flight core only - per-surface lift/drag, thrust, angular
    damping, wind, instrument signals, and the networking to make all of that work on a dedicated server.
    It intentionally has **no** gear/flap/trim controllers, cargo, weapons, or cockpit systems. Those belong
    in a variant built on top of the core.

## Key facts

| | |
|---|---|
| Mod GUID | `69A8A34027DA65C5` |
| Script class prefix | `PFC_` |
| Reference prefab | `Prefabs/Vehicles/Cessna172.et` (inherits `Wheeled_Base.et`) |
| Dependency | Arma Reforger core (`58D0FB3206B6F859`) |

## Project layout

The mod itself lives in `Propeller flight core/`:

| Path | Contents |
|---|---|
| `Scripts/Game/Vehicle/` | Flight model, controller, aero surfaces, networked movement |
| `Scripts/Game/Utilities/` | Pilot helper utilities |
| `Configs/` | Input bindings, control hints, propeller procedural animation |
| `Prefabs/Vehicles/` | The Cessna 172 reference airframe |
| `UI/layouts/HUD/` | Vehicle HUD layout |
| `Worlds/` | Multiplayer test world |

The repo root also hosts the developer documentation (`docs/`, built with MkDocs).

## Documentation

Full developer documentation is published at
**<https://codingpandaren.github.io/enfusion-pfc/>** and covers the architecture, flight model tuning,
prefab setup, input/controls, networking, and how to build your own aircraft variant on top of the core.
