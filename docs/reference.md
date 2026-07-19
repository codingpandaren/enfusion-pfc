# Reference

Attribute defaults below are the **script class defaults** (from `[Attribute(...)]`). The Cessna prefab
overrides several of these - the prefab is authoritative for the reference airframe.

## PFC_FlightModel

| Attribute | Default | Description |
|---|---|---|
| `m_fMaxThrustPerEngine` | 2500 | Max thrust per engine (N). |
| `m_fMinHealthThrustFraction` | 0.25 | Thrust fraction left at zero hull health (linear with health). |
| `m_iNumEngines` | 1 | Number of engines. |
| `m_fMaxControlDeflectionDeg` | 25 | Max control-surface deflection (deg). |
| `m_fFlapDeflectionDeg` | 40 | Flap deflection when deployed (deg). Core never deploys flaps. |
| `m_fFlapDeployDurationSeconds` | 2.0 | Flap deploy/retract time (s). |
| `m_fAirDensitySeaLevel` | 1.225 | Air density at sea level (kg/m³). |
| `m_fDensityFalloff` | 0.0001 | Linear density falloff per metre of altitude. |
| `m_fWindScale` | 1.0 | Multiplier on world wind (0 = ignore, >1 = stronger). |
| `m_fWindSurfaceFraction` | 0.5 | Wind fraction at the surface (boundary-layer gradient). |
| `m_fWindGradientHeight` | 150 | AGL (m) at which wind reaches full strength. |
| `m_fGustIntensity` | 0.3 | Gust strength as a fraction of steady wind (0 = smooth). |
| `m_fGustVerticalScale` | 0.5 | Vertical gust strength relative to horizontal. |
| `m_fFuselageDragArea` | 0.6 | Fuselage longitudinal drag (Cd·A, m²). |
| `m_fAngularDamping` | 2.0 | Angular-velocity decay rate (1/s), speed-scaled. |
| `m_bDebugDraw` | false | Initial F6 debug-draw state. |
| `m_fIdleRPM` | 800 | Engine idle RPM. |
| `m_fMaxRPM` | 2700 | Engine max RPM. |
| `m_fRPMRate` | 0.5 | RPM ramp rate (fraction of max RPM per second). |
| `m_fAltAGLSmoothingTau` | 0.5 | Radar-altimeter smoothing tau (s). |
| `m_aSurfaceDefs` | - | Array of `PFC_AeroSurfaceDef` (the aero surfaces). |

### Variant hooks (protected virtual)

Overridable by a `PFC_FlightModel` subclass; base implementations reproduce the stock prop behaviour. See
[Architecture → Variant hooks](architecture.md#variant-hooks) and
[Building a Variant → Extending the flight model itself](building-a-variant.md#extending-the-flight-model-itself).

| Hook | Purpose |
|---|---|
| `GetSurfaceDeflection(axis, pitch, roll, yaw, maxDef, out deflection)` | Axis → surface deflection mapping; `false` = surface undriven. Extra axes via `modded enum PFC_ControlAxis`. |
| `GetSurfaceAirVelocityLS(owner, physics, surfLocalPos, velocityLS)` | Per-surface local airflow (base: CoM airflow for all surfaces). |
| `GetFuselageDragArea(speed, airDensity)` | Fuselage drag area (base: `m_fFuselageDragArea`). |
| `UpdateEngineSpool(throttle, timeSlice, destroyed)` | RPM spool toward target (base: linear `m_fRPMRate` ramp). |
| `ComputeThrustMagnitude(speed, airDensity, destroyed)` | Total thrust in N (base: idle→max fraction × max thrust × engines × health). |
| `GetThrustHealthMultiplier()` | Damage → power scaling. |
| `OnAeroSimulate(owner, physics, timeSlice, speed, aoaDeg, airDensity)` | End-of-tick extension point on the authoritative peer (base: empty). |

## PFC_FlightController

| Attribute | Default | Description |
|---|---|---|
| `m_fThrottleRate` | 0.5 | Throttle change per second while held. |
| `m_fControlRate` | 1.0 | Roll input slew rate (units/s). |
| `m_fPitchControlRate` | 0.7 | Pitch input slew rate (units/s). |
| `m_fYawControlRate` | 1.5 | Yaw input slew rate (units/s). |
| `m_fPitchInputScale` | 0.7 | Max pitch input magnitude (0..1). |
| `m_fGroundSteerScale` | -1.0 | Nose-wheel steering scale from `PFC_Yaw`; 0 disables. |
| `m_fInputSendIntervalMs` | 16 | Milliseconds between input RPCs to the server. |

_Cessna prefab overrides:_ `m_fControlRate 3.5`, `m_fPitchControlRate 3`, `m_fYawControlRate 3.5`,
`m_fPitchInputScale 1`.

## PFC_AeroSurfaceDef

| Attribute | Default | Description |
|---|---|---|
| `m_sLabel` | "Surface" | Label for logs / debug gizmos. |
| `m_vPosition` | 0 0 0 | Local position (m). Mirror negates X. |
| `m_vRotation` | 0 0 0 | Local rotation (yaw pitch roll, deg) → normal + forward axes. |
| `m_bMirror` | true | Spawn an X-mirrored counterpart (ailerons flip sign). |
| `m_iControlAxis` | NONE | `NONE` / `ELEVATOR` / `AILERON` / `RUDDER` / `FLAPS`. |
| `m_fChord` | 4.5 | Chord length (m). |
| `m_fSpan` | 8.0 | Span length (m). |
| `m_fLiftSlope` | 5.5 | Lift-curve slope per radian (thin-airfoil max ≈ 2π). |
| `m_fSkinFriction` | 0.02 | Skin-friction drag at zero lift. |
| `m_fZeroLiftAoA` | 0 | Zero-lift angle of attack (deg). |
| `m_fStallAngleHigh` | 15 | Positive stall angle (deg). |
| `m_fStallAngleLow` | -15 | Negative stall angle (deg). |
| `m_fFlapFraction` | 0 | Fraction of chord that is the moving control (0 = fixed). |
| `m_fEfficiency` | 0.85 | Oswald efficiency factor (induced drag). |
| `m_fAspectRatioOverride` | 0 | Override AR for induced drag; 0 = use span/chord. |

## Instrument signals

Published by `PFC_FlightModel.UpdateSignals` (names match vanilla `VehicleGauge_*.conf`). MP signals
replicate to all peers; per-engine prop angles are local, integrated from the replicated RPM.

| Signal | Unit | Compression |
|---|---|---|
| `airspeed` | km/h | None |
| `altitude` | m | None |
| `altitudeAGL` | m | None |
| `climbRate` | m/s | None |
| `groundspeed` | km/h | None |
| `gLoad` | g | None |
| `RPM` | 0..1 | Range01 |
| `MainRotorRPMScaled` | 0..1 (audio) | Range01 |
| `EngineRPM_<n>` | 0..1 | Range01 |
| `PropellerAngle_<n>` | deg | local |
| `ElevatorAngle` / `RudderAngle` / `AileronAngle` / `FlapAngle` | deg | RotDEG |
| `pitch` / `bank` / `heading` | deg | None |

## GUIDs

| Resource | GUID |
|---|---|
| PFC mod | `69A8A34027DA65C5` |
| Arma Reforger core (dependency) | `58D0FB3206B6F859` |
| `Wheeled_Base.et` (parent prefab) | `62F416029692CE40` |
| Vanilla `chimeraInputCommon.conf` (reuse in meta) | `795184CF9AD764DB` |
| Vanilla control-hints `AvailableActions.conf` (reuse in meta) | `80CC0413DDBDFCB9` |

PFC's own resources use the `69A8A340…` GUID range. Input-action source GUIDs follow a convention:
`…0002xxx` = keyboard, `…0003xxx` = gamepad, `…0004xxx` = joystick.
