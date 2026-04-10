# Motion architecture — migration sandbox

Reference copies of the legacy `esc_inertia` code.
**These files are NOT compiled** (outside `src/`).

**Source code deleted from `src/` on 2026-04-10** — sandbox files are the
sole remaining reference for features not yet in `motion`.

## Purpose

As each feature is re-implemented in the new `motion` architecture, the
corresponding section is **deleted from these sandbox files**.  When a
sandbox file is empty (or contains only a `// DROPPED` comment), the
migration for that layer is complete.

## Migration status (2026-04-10 audit)

### Already migrated (active in `motion.h / .cpp / motion_struct.h / motion_presets.h`)

- [x] `MotionMode` enum → implicit (ramp vs gear+inertia pointer pattern)
- [x] `EscInertiaConfig` struct → split into `MotionHwMargin`, `MotionMargin`, `MotionDeadBand`, `MotionRamp`, `MotionGear`, `MotionInertia`, `MotionConfig`
- [x] `EscInertiaRuntime` struct → `MotionRuntime`
- [x] `EscInertiaState` struct → `MotionOutput`
- [x] `EscInertiaInputs` struct → removed (caller fills raw ComBus value)
- [x] `esc_inertia_update()` → `motion_update()`
- [x] `RAMP_SIMPLE` path → `motion_process()` with `ramp != nullptr`
- [x] Traction ramp with 3-gear timing → `motion_process()` with `gear + inertia != nullptr`
- [x] `isBraking / inReverse / isDriving` → `MotionOutput`
- [x] `currentSpeed` → `MotionOutput::currentSpeed` (combus_t units)
- [x] ComBus write → caller responsibility (machine loop)
- [x] `kMotion_Traction_Truck` → `kTraction_Heavy` (layered sub-configs)
- [x] `kMotion_Steer` → `kSteer_Electric_heavy` (MotionRamp only)

### NOT yet migrated — features to add to `motion` when needed

| Feature | `esc_inertia` location | Purpose | Priority |
|---|---|---|---|
| `EscLinearizeFn` callback | `esc_inertia_struct.h` | ESC output curve correction for non-linear ESCs (QuicRun etc.) | Low — no non-linear ESC in use |
| `crawlerRampTimeMs` | `EscInertiaConfig` | Direct-control mode with ~2ms ramp (bypass inertia) | Medium — useful for precision positioning |
| `lowRangePct` | `EscInertiaConfig` | Ramp time scaling for low-range transfer case (e.g. 50% = half speed) | Low — no transfer case in use |
| `autoRevAccelPct` | `EscInertiaConfig` | Acceleration boost in automatic reverse (200 = 2× faster) | Low |
| `cbusMaxLimit / cbusMinLimit` | `EscInertiaConfig` | Sanity cap — reject values outside valid range | Medium — good defensive input validation |
| `airBrakeTrigger` | `EscInertiaState` | One-shot pulse when inertial motion stops → triggers air brake sound | High — needed for sound migration |
| `brakeDetect` | `EscInertiaState` | Stick and inertia position on opposing sides → brake light / sound | High — needed for sound migration |
| 5-state driveState FSM | `esc_inertia.cpp` | Explicit states (standing/fwd/braking-fwd/rev/braking-rev) — richer than current bidirectional ramp | Medium — current motion works but lacks transition logic |
| `currentSpeed` 0–500 scale | `EscInertiaState` | DiYGuy sound engine expects 0–500 range, not combus_t | High — adapt when sound reads MotionOutput |
| `kMotion_Hydraulic_Slow` | `esc_presets.h` | Slow hydraulic cylinder preset (200ms ramp) | Medium — add when hydraulic uses motion |

### Sandbox reference files

| Sandbox file | Former source location (deleted) |
|---|---|
| `esc_inertia_struct.h` | `include/struct/esc_inertia_struct.h` |
| `esc_inertia.h` | `src/core/system/hw/esc_inertia.h` |
| `esc_inertia.cpp` | `src/core/system/hw/esc_inertia.cpp` |
| `esc_presets.h` | `src/core/config/hw/esc/esc_presets.h` |
