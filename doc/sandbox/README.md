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

| Feature | `esc_inertia` location | Purpose | Status |
|---|---|---|---|
| `airBrakeTrigger` | `esc_inertia_struct.h` / `.cpp` | One-shot pulse when inertial motion stops → air brake sound | **Migrated** — `MotionOutput::airBrakePulse` |
| `brakeDetect` | `esc_inertia_struct.h` / `.cpp` | Stick vs inertia opposition → brake light / sound | **Migrated** — `DriveState::kBrakeFwd / kBrakeRev` |
| `currentSpeed` 0–500 scale | `EscInertiaState` | DiYGuy sound engine expects 0–500, not combus_t | **Deferred** — handle during sound module refactor (combus_t native) |
| 5-state driveState FSM | `esc_inertia.cpp` | Explicit states (standing/fwd/braking-fwd/rev/braking-rev) | **Migrated** — `DriveState` namespace + `MotionRuntime::driveState` |
| `kMotion_Hydraulic_Slow` | `esc_presets.h` | Slow hydraulic cylinder preset (200ms ramp) | Medium — add when hydraulic uses motion |
| ~~`autoRevAccelPct`~~ | — | ~~Acceleration boost in auto reverse~~ | **Dropped** — not needed |
| ~~`EscLinearizeFn`~~ | — | ~~ESC output curve for non-linear ESCs~~ | **Noted** — TODO in `motion.cpp` step 6 |
| ~~`kMotion_Hydraulic_Slow`~~ | — | ~~Slow hydraulic preset (200ms ramp)~~ | **Dropped** — trivial to recreate as MotionRamp |
| ~~`lowRangePct`~~ | — | ~~Transfer case low-range scaling~~ | **Dropped** — not needed |
| ~~`crawlerRampTimeMs`~~ | — | ~~Direct-control ~2ms ramp~~ | **Dropped** — not needed |
| ~~`cbusMaxLimit / cbusMinLimit`~~ | — | ~~Input range sanity cap~~ | **Dropped** — already in motion (`s_clamp`) |

### Sandbox reference files

| Sandbox file | Former source location (deleted) |
|---|---|
| `esc_inertia_struct.h` | `include/struct/esc_inertia_struct.h` |
| `esc_inertia.h` | `src/core/system/hw/esc_inertia.h` |
| `esc_inertia.cpp` | `src/core/system/hw/esc_inertia.cpp` |
| `esc_presets.h` | `src/core/config/hw/esc/esc_presets.h` |
