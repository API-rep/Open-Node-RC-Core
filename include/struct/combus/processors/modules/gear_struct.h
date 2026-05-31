/*!****************************************************************************
 * @file  gear_struct.h
 * @brief Virtual gearbox module — config and state structures.
 *
 * @details Shared between FSM primitives (`gear_fsm`) and CbProc wrappers
 *   (`cb_gear`).  Includes:
 *   - GearStepCfg / SubGearStepCfg / GearShiftProfile — shift threshold profiles
 *   - GearFsmState — runtime FSM state (gear, prevRpm, shift guard, sub-gear)
 *   - GearProcCfg — CbProc config (profile pointer)
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
 *   `gearRatio` (in ‰) encodes the mechanical gear ratio relative to top gear:
 *   `gearRatio = round(topBoxRatio / thisBoxRatio × 1000)`.
 *   `gear_ratio_fn` applies: `value = value × gearRatio / 1000`.
 *   gear[last].gearRatio = 1000 (direct drive / 1:1 reference).
 *   `maxRpm` of the profile equals the last gear's `upShift` by convention.
 */
struct GearStepCfg {
    int16_t  upShift;           ///< RPM threshold to upshift from this gear (= maxRpm for last gear).
    int16_t  downShift;         ///< RPM threshold to downshift to this gear (coasting).
    int16_t  downShiftBraking;  ///< RPM threshold to downshift to this gear (braking — higher → earlier).
    uint16_t rampTime;          ///< Inertia ramp duration (ms) — written to CbRampCfg::rampTimeMs via dynCfg.
    uint16_t gearRatio;         ///< Gear ratio in ‰ — gear_ratio_fn scales: value × gearRatio / 1000.
};

/**
 * @brief Config for one sub-gear step.
 *
 * @details Index 0 = slowest crawl, index N-1 = fastest crawl.
 *   `maxSpeedPct` caps the wheel speed output via the `subgear-speed` proc.
 *   RPM (sound) flows freely from stick by default.
 *
 *   Cruise-control mode (hold+nudge) is a proc-level option — see `CbCruiseCfg`
 *   in `cb_cruise.h` (motion proc layer, independent of the gear module).
 */
struct SubGearStepCfg {
    uint16_t rampTime;    ///< Inertia ramp duration (ms) for this sub-gear.
    uint8_t  maxSpeedPct; ///< Wheel speed ceiling as % of full-range max speed (0–100).
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

    uint16_t upshiftDampSteps = 0u;     ///< extAccelSteps applied to traction ramp on upshift (0 = disabled).
                                        ///<   gear_upshift_damp_fn writes this value to CbRampCfg::extAccelSteps
                                        ///<   for upshiftDampMs after each upshift event.
    uint16_t upshiftDampMs    = 0u;     ///< Duration of upshift accel damping (ms). 0 = no damping.
};


// =============================================================================
// 2. FSM STATE
// =============================================================================

/**
 * @brief Mutable runtime state for the gear FSM.
 *
 * @details Zero-init is valid (gear = 0 sentinel triggers auto-init to gear 1).
 *   Assigned to `CbProc::state` for `gear_fsm_fn` and `gear_ratio_inv_fn`
 *   (shared — `gear_ratio_inv_fn` reads `gear` read-only).
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
// 3. DAMP STATE
// =============================================================================

/**
 * @brief Runtime state for the upshift accel-damping proc.
 *
 * @details Zero-init is valid.  Assigned to `CbProc::state` for
 *   `gear_upshift_damp_fn`; cast back inside the proc.
 */
struct GearDampState {
    uint8_t  prevGear  = 0u;   ///< Gear seen last cycle — upshift detection.
    uint32_t dampEndMs = 0u;   ///< millis() when damping window expires. 0 = no active window.
};


// =============================================================================
// 4. CBPROC CONFIGS
// =============================================================================

/**
 * @brief CbProc config for gear module functions.
 *
 * @details Assigned to `CbProc::cfg` (as `const void*`); cast back to
 *   `const GearProcCfg*` inside each fn.
 *
 *   Used by: `gear_fsm_fn`, `gear_ratio_inv_fn`, `gear_ratio_fn`,
 *   `gear_subgear_cap_fn`, `gear_dir_fn`, `gear_dyn_ramp_fn`,
 *   `gear_upshift_damp_fn`.
 */
struct GearProcCfg {
    const GearShiftProfile* profile; ///< Shift threshold profile — pointer via vehicle alias.
};

// EOF gear_struct.h
