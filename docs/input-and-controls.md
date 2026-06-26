# Input & Controls

PFC adds its own input actions and merges them into the vanilla `CarContext`, so they're active whenever the
player is the active pilot. `PFC_FlightController` reads these, smooths them with per-axis slew limits, and
exposes them to the flight model.

## Actions & default bindings

Defined in `Configs/System/chimeraInputCommon.conf`:

| Action | Keyboard | Gamepad | Joystick |
|---|---|---|---|
| `PFC_Pitch` | ++w++/++s++, ++up++/++down++ | left stick Y | `axis1` |
| `PFC_Roll` | ++a++/++d++, ++left++/++right++ | left stick X | `axis0` |
| `PFC_Yaw` | ++q++/++e++ | left/right trigger | `axis5` (twist) |
| `PFC_ThrottleUp` | ++shift++ | RB (`shoulder_right`) | `button4` |
| `PFC_ThrottleDown` | ++ctrl++ | LB (`shoulder_left`) | `button5` |

Each analog axis (`Pitch`/`Roll`/`Yaw`) is built as an `InputSourceSum` of signed `+`/`-` halves, following
the vanilla convention (the negative half carries an `InputFilterValue` `Multiplier -1`).

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
- **Injects nose-wheel steering** - writes the smoothed `PFC_Yaw` value into `CarSteering` (scaled by
  `m_fGroundSteerScale`, default `-1.0`), so ++q++/++e++ steer the nose wheel on the ground while ++a++/++d++
  stay roll-only. Set `m_fGroundSteerScale` to `0` to disable (then ++a++/++d++ steer as vanilla).