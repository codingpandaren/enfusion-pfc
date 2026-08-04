# Input & Controls

PFC defines the shared **`Airplane_*` action set** and the **"Airplane" tab** in the Controls menu. The
actions merge into the vanilla `CarContext`, so they're active whenever the player is the active pilot of a
car-based aircraft. Downstream flight mods (Jet Flight Core and the aircraft built on it or on PFC) read the
same actions, so every fixed-wing aircraft shares one binding set and one controls tab.

`PFC_FlightController` reads the flight axes, smooths them with per-axis slew limits, and exposes them to
the flight model.

## Flight actions & default bindings

Defined in `Configs/System/chimeraInputCommon.conf`. These are the actions the PFC core itself reads:

| Action | Keyboard | Gamepad | Joystick |
|---|---|---|---|
| `Airplane_Pitch` | ++w++/++s++ | right stick Y | `axis1` (push = nose down) |
| `Airplane_Roll` | ++a++/++d++ | right stick X | `axis0` |
| `Airplane_Yaw` | ++q++/++e++ | left stick X | `axis5` (twist) |
| `Airplane_ThrottleUp` | ++shift++ | left stick up | `button4` (hold) |
| `Airplane_ThrottleDown` | ++ctrl++ | left stick down | `button5` (hold) |
| `Airplane_ThrottleAxis` | - | - | throttle lever (second device, `axis2`) |

The gamepad layout mirrors the vanilla **helicopter** scheme - right stick is pitch/roll, left stick is
throttle (vertical) and rudder (horizontal) - so a heli player is immediately at home, and the triggers and
bumpers stay free. Joystick axis numbers are generic defaults and vary by device; players remap in the
Controls menu.

Each analog axis (`Pitch`/`Roll`/`Yaw`) is built as an `InputSourceSum` of signed `+`/`-` halves, following
the vanilla convention (the negative half carries an `InputFilterValue` `Multiplier -1`).

## Systems actions (for variants)

The shared set also defines systems actions the core itself never reads. Variant controllers bind to these
so every aircraft answers to the same keys and shows up in the same tab:

| Action | Purpose |
|---|---|
| `Airplane_GearToggle` | Landing gear up/down |
| `Airplane_Flaps` | Flaps toggle |
| `Airplane_Airbrake` | Airbrake toggle |
| `Airplane_ReverseThrust` | Reverse thrust / beta range |
| `Airplane_TrimUp` / `Airplane_TrimDown` / `Airplane_TrimReset` | Pitch trim |
| `Airplane_WheelBrake` / `Airplane_WheelBrakePersistent` | Wheel brake (hold / parking) |
| `Airplane_EngineStart` / `Airplane_EngineStop` | Engine start/stop |
| `Airplane_LightsTaxiToggle` / `Airplane_LightsLandingToggle` | Taxi / landing lights |

## The Airplane controls tab

`Configs/System/keyBindingMenu.conf` adds one `SCR_KeyBindingCategory` named **Airplane**, with separator
rows splitting it into *Flight Controls*, *Systems*, and *Advanced*.

!!! warning "Resource-merge pitfall"
    The engine loads exactly **one** copy of each of these configs, so a mod's version only merges if its
    `.meta` reuses the *vanilla* resource identity instead of a freshly generated one. This applies to
    `chimeraInputCommon.conf`, the control-hints `AvailableActions.conf`, and `keyBindingMenu.conf` (see the
    `.meta` files shipped with PFC). With a fresh identity nothing errors - the actions simply never load
    (`GetActionValue` returns 0 forever) or the controls tab never appears.

!!! note "Cold start only"
    Input configs merge at cold engine start. A Workbench "Reload Game" or resource reimport does not
    re-merge them - do a full restart after editing.

## Input smoothing

`PFC_FlightController` doesn't pass raw input to the model - it slews each axis toward its target at a
per-axis rate, so control response is smooth and a little "heavy" like a real aircraft:

| Attribute | Default (prefab) | Meaning |
|---|---|---|
| `m_fControlRate` | 3.5 | Roll slew rate (units/sec; 1.0 = full deflection in 1 s) |
| `m_fPitchControlRate` | 3 | Pitch slew rate (slower - heavy elevator) |
| `m_fYawControlRate` | 3.5 | Yaw slew rate |
| `m_fPitchInputScale` | 1 | Caps max pitch input magnitude (0..1) |
| `m_fThrottleRate` | 0.5 | Throttle change per second while held |

!!! note
    The component's *attribute defaults* differ from the values set on the Cessna prefab (shown above). The
    prefab is the source of truth for the reference airframe.

## Ground steering & key de-conflicting

Because the airframe rides on `Wheeled_Base.et`, its native car controls collide with the flight keys.
`PFC_FlightController.ApplyGroundSteering()` runs in `EOnFrame` (the wheel sim consumes `CarSteering` before
the simulate phase, so writing it later is ignored) and:

- **Zeroes `CarThrust`** - otherwise ++w++ revs the car engine and you hear the leftover turbine spin up.
- **Zeroes `CarBrake`** - otherwise ++s++ (pitch back) grabs the wheel brake.
- **Zeroes `CarShift`** - otherwise ++q++/++e++ (yaw) shift the gearbox.
- **Injects nose-wheel steering** - writes the smoothed `Airplane_Yaw` value into `CarSteering` (scaled by
  `m_fGroundSteerScale`, default `-1.0`), so ++q++/++e++ steer the nose wheel on the ground while ++a++/++d++
  stay roll-only. Set `m_fGroundSteerScale` to `0` to disable (then ++a++/++d++ steer as vanilla).

## Free look

`PFC_FreeLookController` gives car-based aircraft the helicopter-style camera:

- **Mouse & keyboard** - the mouse always free-looks, no modifier needed.
- **Gamepad** - the camera stays locked to the nose. Hold the vanilla **Freelook** modifier (right bumper)
  and the right stick pans the camera instead of flying; release and the camera *stays* where you left it;
  tap the modifier to recenter. While the modifier is held the flight controller ignores right-stick
  pitch/roll (yaw and throttle on the left stick stay live).

Setup needs **two pieces, both required**:

1. Add the `PFC_FreeLookController` component to the vehicle prefab.
2. Set `ForcedFreeLook 1` on the **pilot compartment slot**.

The slot's `ForcedFreeLook 1` keeps the camera following the head aim, so a pan is never auto-recentered.
The controller then drives the character's freelook state per device every frame: forced free look for
mouse & keyboard, cleared for gamepad unless the Freelook modifier is held (which is what keeps the right
stick from panning without the modifier).

!!! warning "A static slot value alone is not enough"
    On a car-based aircraft no single `ForcedFreeLook` value gives the heli feel: `1` alone frees the
    camera for *both* devices (gamepad pans without any modifier), while `0` locks mouse look *and*
    auto-recenters gamepad pans the moment the modifier is released. The per-device controller plus
    `ForcedFreeLook 1` is the working combination.
