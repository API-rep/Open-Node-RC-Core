/*!****************************************************************************
 * @file  gear_struct.h
 * @brief Virtual gearbox module — config and state structures.
 *
 * @details Shared between FSM primitives (`gear_fsm`) and CbProc wrappers
 *   (`cb_gear`).  Includes:
 *   - GearStepCfg / SubGearStepCfg / GearShiftProfile — shift threshold profiles
 *   - GearFsmState — runtime FSM state (gear, prevRpm, shift guard, sub-gear)
 *   - GearProcCfg — CbProc config (profile pointer)
 *   - ShiftDeltaState — shift-delta runtime (upshift edge detection)
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. GEAR SHIFT PROFILE  (shared between FSM and motion presets)
// =============================================================================

/**
 * @brief Threshold and inertia config for one virtual gear.
 *
 * @details `upShift` is the RPM at which the FSM transitions from this gear to
 *   the next.  `downShift` / `downShiftBraking` are the thresholds below which
 *   the FSM drops back one gear (coasting / braking respectively).
 *   Hysteresis requirement: `upShift > downShift` — prevents gear hunting.
 *
 *   `shiftDelta` is the RPM drop applied when upshifting INTO this gear:
 *   the virtual RPM used by the FSM drops by this amount at the moment of
 *   the shift, simulating the engine RPM fall as the new ratio takes effect.
 *   gear[0].shiftDelta is ignored (no upshift into gear 1).
 *   `maxRpm` of the profile equals the last gear's `upShift` by convention.
 */
struct GearStepCfg {
    int16_t  upShift;           ///< RPM threshold to upshift from this gear (= maxRpm for last gear).
    int16_t  downShift;         ///< RPM threshold to downshift to this gear (coasting).
    int16_t  downShiftBraking;  ///< RPM threshold to downshift to this gear (braking — higher → earlier).
    uint16_t rampTime;          ///< Inertia ramp duration (ms) — written to CbRampCfg::rampTimeMs via dynCfg.
    int16_t  shiftDelta;        ///< RPM drop when upshifting INTO this gear (ignored for gear 1).
};

/**
 * @brief Config for one sub-gear step.
 *
 * @details Index 0 = slowest crawl, index N-1 = fastest crawl.
 *   All sub-gear ramp times are slower than normal gear-1 `rampTime`.
 *   `maxPct` caps the RPM input to the gear FSM while this sub-gear is active:
 *   expressed as a percentage (0–100) of `gear[0].upShift` (the gear-1 RPM ceiling).
 *   Full throttle in sub-gear n maps to `gear[0].upShift × maxPct / 100`.
 */
struct SubGearStepCfg {
    uint16_t rampTime;  ///< Inertia ramp duration (ms) for this sub-gear.
    uint8_t  maxPct;    ///< RPM ceiling as % of gear[0].upShift (0–100).
};

/**
 * @brief Speed-threshold profile for a virtual N-speed gearbox FSM.
 *
 * @details All speed values are in RPM units.
 *   `gear[n]` covers the (n+1)-th gear (0-based index).
 *   `gear[n].upShift` is the RPM threshold to shift from gear n+1 to n+2.
 *   `gear[gearCount-1].upShift` is the maximum RPM (scaling reference).
 *   Hysteresis requirement: `gear[n].upShift > gear[n].downShift`.
 *
 *   Presets are declared `constexpr` in `simulation_presets.h` and exposed
 *   via vehicle aliases (`*_motion.h`).
 */
struct GearShiftProfile {
    uint8_t               gearCount;    ///< Number of active gears (= std::size(gear[])).
    const GearStepCfg*    gear;         ///< Per-gear config — array[gearCount].
    uint16_t              shiftGuardMs; ///< Minimum interval between consecutive shifts (ms).

    uint8_t               subGearCount; ///< Number of sub-gears in gear 1 (0 = sub-gear disabled).
    const SubGearStepCfg* subGear;      ///< Per-sub-gear config — array[subGearCount]; nullptr when subGearCount == 0.
};


// =============================================================================
// 2. FSM STATE
// =============================================================================

/**
 * @brief Mutable runtime state for the gear FSM.
 *
 * @details Zero-init is valid (gear = 0 sentinel triggers auto-init to gear 1).
 *   Assigned to `CbProc::state` for `gear_fsm_fn`; cast back inside the proc.
 */
struct GearFsmState {
    int8_t   gear;         ///< Current virtual gear (1–N).
    int16_t  prevRpm;      ///< RPM seen last cycle — trend detection (rising = accel, falling = decel).
    uint32_t lastShiftMs;  ///< Timestamp of last shift — anti-hunting guard.

    int8_t   subGear;        ///< Active sub-gear index (1..subGearCount). 0 = sub-gear mode inactive.
    bool     prevSubGearSet; ///< Previous state of SUBGEAR_SET — rising-edge detection.
    bool     prevSubGearUp;  ///< Previous state of SUBGEAR_UP  — rising-edge detection.
    bool     prevSubGearDn;  ///< Previous state of SUBGEAR_DOWN — rising-edge detection.
};


// =============================================================================
// 3. CBPROC CONFIGS
// =============================================================================

/**
 * @brief CbProc config for gear module functions.
 *
 * @details Assigned to `CbProc::cfg` (as `const void*`); cast back to
 *   `const GearProcCfg*` inside each fn.
 *
 *   Used by: `gear_fsm_fn`, `gear_upshift_drop_fn`, `gear_rpm_to_speed_fn`,
 *   `gear_dyn_ramp_fn`.
 */
struct GearProcCfg {
    const GearShiftProfile* profile; ///< Shift threshold profile — pointer via vehicle alias.
};

/**
 * @brief Mutable runtime state for `gear_upshift_drop_fn`.
 *
 * @details Assigned to `CbProc::state` (as `void*`); cast back inside
 *   `gear_upshift_drop_fn()`.  Zero-init is valid (prevGear = 0 means
 *   no upshift on first cycle).
 */
struct ShiftDeltaState {
    int8_t prevGear; ///< Gear seen last cycle — upshift edge detection.
};

// EOF gear_struct.h
