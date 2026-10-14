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
smooth motion. Proxies don't simulate aero; they render from a **timestamped snapshot buffer** and drive their
own propeller visuals from the replicated, normalized engine-RPM signal.

How the proxy path works (snapshot interpolation, not dead reckoning):

- The owner stamps each state broadcast with its **sender time**; the receiver estimates the sender-clock
  offset by tracking the least-delayed packets (slow upward drift absorbs clock skew and route changes) and
  measures arrival jitter with an EMA.
- Proxies render the aircraft a **playout delay** in the past (`1.5 × send interval + 3 × jitter`, clamped to
  `m_fPlayoutMinMs..m_fPlayoutMaxMs`), interpolating **between two buffered snapshots**: Hermite position
  (positions + velocities as tangents, so turns stay curved), quaternion lerp for orientation. Because the
  render point sits behind the newest data, network jitter disappears into the buffer instead of becoming
  rubberbanding — remote planes are a small constant time offset, not a jumping target.
- If the buffer runs dry (packet-loss burst), the proxy extrapolates on velocity up to
  `m_fMaxExtrapolationSec`, then fades velocity to zero instead of freezing.
- Discontinuities (loss recovery, ownership handoff) are folded into a **decaying visual offset**
  (`m_fPositionCorrectionRate` / `m_fRotationCorrectionRate`, 1/s) — position and rotation blend back
  smoothly. Only offsets beyond `m_fHardSnapThresholdMeters` / `m_fHardSnapRotationDeg` hard-snap.
- `F6 → Prop Flight → Network interp debug` shows per-plane buffer depth, delay/jitter estimates, playout
  delay, extrapolation state, and absorb/snap counters.

The state RPC carries the sender timestamp, so **all peers must run the same PFC version** after an update to
this component (mismatched RPC signatures fail to marshal).

## Instrument signals

The owner publishes gauge signals via `SignalsManagerComponent` for its **own** cockpit instruments and HUD.

!!! danger "Replication is server → client only"
    Per the engine's replication model (see the BI Replication Overview), state only ever flows
    **server → clients**. A signal written on a pilot-owned client exists on that client alone — it does
    NOT replicate to the server or to other clients, no matter how it was registered. Anything another
    peer must see has to travel through a server-routed channel (owner → `RplRcver.Server` RPC →
    `RplRcver.Broadcast`, or the movement-state snapshot) and be written into a **local** signal on each
    receiving peer.

    This is why control-surface / flap / airbrake visuals ride the `PFC_NwkMovementComponent` snapshot
    (pilot control state is packed into the state RPC, interpolated in the snapshot buffer, and every
    proxy writes the surface signals locally) instead of `AddOrFindMPSignal`.

!!! danger "Use the right compression function"
    `AddOrFindMPSignal(..., SignalCompressionFunc.X)` - the compression must match the value range:

    - **Raw units** (airspeed km/h, altitude m, climb m/s, pitch/bank/heading deg, G-load): use `None`.
      `Range01` would clamp them to `[0,1]` over the wire, pinning every receiver's gauge at `1`.
    - **Normalized 0..1** (throttle, normalized RPM): `Range01` is fine.
    - **Angles**: `RotDEG`.

## Boot-window guard

Writing an MP signal - or firing an RPC - on an entity that isn't yet **registered for replication** spams
`"unregistered item"` errors every frame during the spawn window. PFC guards both:

- Signal publishing is gated on `m_RplComponent.Id().IsValid()`.
- The input RPC is sent only when `!Replication.IsServer()` **and** the Rpl id is valid - if the server owns
  the plane (unmanned), there's no one to send to and it already holds the values locally.

!!! tip "Reuse these patterns in variants"
    Any per-tick RPC or MP-signal write you add in a variant should follow the same two guards
    (owner/server check + valid-Rpl-id check), or you'll reintroduce the boot-window log spam.
