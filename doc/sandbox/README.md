# Motion architecture — migration sandbox

Reference copies of the legacy `esc_inertia` code.
**These files are NOT compiled** (outside `src/`).

## Purpose

As each feature is re-implemented in the new `motion` architecture, the
corresponding section is **deleted from these sandbox files**.  When a
sandbox file is empty (or contains only a `// DROPPED` comment), the
migration for that layer is complete.

## Migration checklist

### `esc_inertia_struct.h` → new `motion_struct.h`

- [ ] `MotionMode` enum → `MotionLayer` enum (extended)
- [ ] `EscLinearizeFn` typedef → kept in `MotionFsm`
- [ ] `EscInertiaConfig` struct → split into `MotionHw`, `MotionMargin`, `MotionBand`, `MotionRamp`, `MotionGear`, `MotionFsm`, `MotionConfig`
- [ ] `EscInertiaRuntime` struct → `MotionRuntime`
- [ ] `EscInertiaState` struct → `MotionState`

### `esc_inertia.h` → new `motion.h`

- [ ] `EscInertiaInputs` struct → `MotionInputs`
- [ ] `esc_inertia_update()` → `motion_update()`

### `esc_inertia.cpp` → new `motion.cpp`

- [ ] helper `raw_pulse_dir()` → `motion_pulse_dir()` (shared for input + esc pos)
- [ ] helper `esc_pulse_dir()` → merged into above
- [ ] helper `compute_ramp_time()` → `MotionGear` apply step
- [ ] helper `map_esc_signal()` → `MotionFsm` apply step (linearize + scale)
- [ ] passthrough path (`cfg == nullptr`) → Layer 0 default
- [ ] `RAMP_SIMPLE` path → `MotionRamp` apply step
- [ ] `TRACTION_FSM` path → `MotionFsm` apply step (states 0–4)
- [ ] clutch ramp gain logic → `MotionFsm` apply step
- [ ] `currentSpeed` output calculation → `MotionState` fill
- [ ] ComBus write → `motion_update()` tail

### `esc_presets.h` → new `motion_presets.h`

- [ ] `kMotion_Traction_Truck` → rebuilt with layered sub-configs
- [ ] `kMotion_Hydraulic_Slow` → rebuilt with `MotionRamp` only
- [ ] `kMotion_Steer` → rebuilt with `MotionRamp` only

## Files

| Sandbox file | Original location |
|---|---|
| `esc_inertia_struct.h` | `include/struct/esc_inertia_struct.h` |
| `esc_inertia.h` | `src/core/system/hw/esc_inertia.h` |
| `esc_inertia.cpp` | `src/core/system/hw/esc_inertia.cpp` |
| `esc_presets.h` | `src/core/config/hw/esc/esc_presets.h` |
