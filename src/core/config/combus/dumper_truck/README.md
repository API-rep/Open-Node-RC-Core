# dumper_truck — vehicle class profiles

This directory contains *all* configuration specific to the
**dumper truck / articulated rigid hauler** vehicle class, independently
of which circuit board (machine node or sound node) is compiling it.

---

## Files

| File | Purpose |
|---|---|
| `dumper_truck_ids.h` | `AnalogComBusID` / `DigitalComBusID` enum values |
| `dumper_truck.h` / `.cpp` | `ComBus` struct and channel arrays |
| `sound/dumper_truck_sound.h` / `.cpp` | **Sound engine dynamics** (acc, dec, CEP, shift thresholds) |
| `dumper_truck_motion.h` | **Traction motion preset alias** (`kDumperTruckTractionPreset`) |
| `inputs_map/` | Input-device → com-bus mapping |

---

## Sound profile (`sound/dumper_truck_sound.h`)

The active profile is routed by `MACHINE_TYPE` through
`src/core/config/combus/sound_dynamics.h`, which exposes the
canonical symbol `kVehicleSoundDynamics`.  `main.cpp` and legacy vehicle
headers include `sound_dynamics.h` via `<core/config/combus/sound_dynamics.h>` —
they never reference the concrete header directly.

### Migrated parameters

The following behavioural parameters were extracted from
`src/sound_module/vehicles/CaboverCAT3408.h` and are now owned by
`kVehicleSoundDynamics` (see `sound/dumper_truck_sound.h`):

| Source variable | New field | Notes |
|---|---|---|
| `acc` | `engineAcc` | Engine mass simulation accel step |
| `dec` | `engineDec` | Engine mass simulation decel step |
| `clutchEngagingPoint` | `clutchEngagingPoint` | Default value — overridable in AIRPLANE_MODE |
| `MAX_RPM_PERCENTAGE` | `maxRpmPercentage` | Default value — overridable at runtime |
| `semiAutoUpShift[3]` | `upShift[3]` | SEMI_AUTO gear upshift speed thresholds |
| `semiAutoDownShift[3]` | `downShift[3]` | SEMI_AUTO downshift thresholds — coasting |
| `semiAutoDownShiftBraking[3]` | `downShiftBraking[3]` | SEMI_AUTO downshift thresholds — braking |

All fields are accessible via `kVehicleSoundDynamics.<field>`.

### Parameters that remain in `CaboverCAT3408.h`

These legacy rc_engine_sound ESC simulation variables drive internal logic
inside the sound node and are **not** part of the vehicle class config layer.
They stay in the legacy file pending full removal of the rc_engine_sound
dependency (roadmap step 5 — `ComBusSoundInterpreter`):

- `escRampTimeFirstGear` / `escRampTimeSecondGear` / `escRampTimeThirdGear`
- `escBrakeSteps`
- `escAccelerationSteps`

---

## Motion preset (`dumper_truck_motion.h`)

`kDumperTruckTractionPreset` is an alias for `kTraction_Heavy` (defined in
`src/core/config/hw/motion_presets.h`).  Machine files (e.g.
`volvo_A60H_bruder.cpp`) may reference either symbol interchangeably.
Use `kDumperTruckTractionPreset` in new machine definitions to make the
vehicle-class relationship explicit.

---

## Adding a new vehicle profile

To add a second vehicle class (e.g. `wheel_loader`):

1. **Create** `src/core/config/combus/wheel_loader/` (copy `dumper_truck/`
   as a template).
2. **Create** `wheel_loader_sound.h` / `.cpp` with a
   `WheelLoaderSoundProfile kVehicleSoundDynamics` struct (same field names,
   tuned values).
3. **Create** `wheel_loader_motion.h` with a `kWheelLoaderTractionPreset`
   alias pointing to the appropriate preset from `motion_presets.h`
   (or define a new one in that file).
4. **Update** `1_Vehicle.h` to select the new vehicle header
   (e.g. `#include "vehicles/WheelLoaderLiebherr.h"`).
5. **Add a dispatch branch** in
   `src/core/config/combus/sound_dynamics.h` for
   `MACHINE_TYPE == WHEEL_LOADER`.
