/******************************************************************************
 * @file simulation_presets.h
 * @brief Ready-made GearShiftProfile and CbRampCfg presets.
 *
 * @details All MotionConfig presets have been replaced by the SimDev pipeline
 *   (sim_traction, cb_ramp).  Only the GearShiftProfile (section 3) and
 *   CbRampCfg (section 4) presets are kept here.
 *****************************************************************************/
#pragma once

#include <iterator>                         // std::size

#include <struct/combus/processors/modules/gear_struct.h>      // GearShiftProfile, GearStepCfg, SubGearStepCfg
#include <struct/combus/processors/motion/cb_ramp_struct.h>    // CbRampCfg
#include <core/system/combus/combus_res.h>  // CbusMaxVal, CbusNeutral, pctToCbus


// =============================================================================
// 3. VIRTUAL GEARBOX PRESETS  (GearShiftProfile — speed-threshold shift FSM)
// =============================================================================

/**
 * @brief Virtual 6-speed gearbox preset — heavy wheeled construction vehicle.
 *
 * @details Calibrated against the CAT 3408 operating band (~700–2100 RPM).
 *   Suitable for ADTs and heavy haulers.  Assign via `kDumperTruckGearShift`
 *   in `dumper_truck_motion.h`.
 *
 *   gear[0].downShift = 700 — idle RPM floor (repurposed field; gear_fsm
 *     never downshifts from gear 1 due to the `gear > 1` guard).
 *   upShift thresholds and shiftDelta values from real-vehicle data.
 *   maxRpm = gear[5].upShift = 2100 (CAT 3408 ceiling).
 *
 *   Shift summary (upShift → land RPM after shiftDelta drop):
 *     1→2 : 1750 → 1200  (−550)    4→5 : 1800 → 1350  (−450)
 *     2→3 : 1750 → 1300  (−450)    5→6 : 1850 → 1300  (−550)
 *     3→4 : 1800 → 1250  (−550)
 *
 *   downShiftBraking values are +150 RPM above coasting downShift — tune on hardware.
 *   shiftGuardMs = 2000 — 2 s minimum between consecutive shifts.
 */
static constexpr GearStepCfg kHeavy6_steps[] = {
    //  upShift  downShift  downShiftBraking  rampTime  shiftDelta
    {     1750,        700,                0,        30,          0 },  // gear 1 — downShift = idle RPM floor; no upshift into it
    {     1750,       1100,             1250,        40,        550 },  // gear 2 — RPM drops 550 on 1→2 (land 1200)
    {     1800,       1150,             1300,        50,        450 },  // gear 3 — RPM drops 450 on 2→3 (land 1300)
    {     1800,       1150,             1300,        55,        550 },  // gear 4 — RPM drops 550 on 3→4 (land 1250)
    {     1850,       1200,             1350,        60,        450 },  // gear 5 — RPM drops 450 on 4→5 (land 1350)
    {     2100,       1200,             1350,        70,        550 },  // gear 6 — RPM drops 550 on 5→6 (land 1300); upShift = maxRpm
};

///< Sub-gear steps: ramp times and speed ceilings.
///< RPM (sound) flows freely from stick in all steps (MICROSPEED default).
///< maxSpeedPct caps the wheel speed output (subgear-speed proc in TRACTION chain).
///< Cruise-control mode (hold+nudge) is toggled via CbSubGearCruiseCfg in proc dynCfg — not here.
static constexpr SubGearStepCfg kHeavy6_subSteps[] = {
    //  rampTime  maxSpeedPct
    {  500,   20 },  // sub-1: slow crawl    — 20 % of max speed
    {  300,   40 },  // sub-2: medium crawl  — 40 % of max speed
    {  150,   60 },  // sub-3: fast crawl    — 60 % of max speed
};

static constexpr GearShiftProfile kGearShift_Heavy6Speed {
    .gearCount        = uint8_t(std::size(kHeavy6_steps)),
    .gear             = kHeavy6_steps,
    .shiftGuardMs     = 2000u,
    .subGearCount     = uint8_t(std::size(kHeavy6_subSteps)),
    .subGear          = kHeavy6_subSteps,
};


// =============================================================================
// 4. RAMP PRESETS  (CbRampCfg — single-axis inertia ramp for hydraulics / steering)
// =============================================================================

/**
 * @brief Ramp preset for the Volvo A60H dump actuators.
 *
 * @details Conservative: 20 ms period, 1 % per step.
 *   Brake is faster than accel — actuator returns quicker when stick released.
 */
static constexpr CbRampCfg kDump_HeavyRamp {
    .rampTimeMs  = 20u,            ///< 20 ms between steps — smooth, no jerk.
    .accelSteps  = pctToCbus(1),   ///< ~1 % per step extending  (~2 s full travel).
    .brakeSteps  = pctToCbus(2),   ///< ~2 % per step retracting (~1 s full travel).
    .neutralBand = 0u,             ///< No extra dead-band (upstream input device handles it).
};

/**
 * @brief Ramp preset for the Volvo A60H steering actuator.
 *
 * @details Mirrors `kSteer_Electric_heavyRamp` from MotionRamp but as a
 *   CbRampCfg for the cb_ramp proc.  Symmetric at 2 % per step.
 */
static constexpr CbRampCfg kSteer_HeavyRamp {
    .rampTimeMs  = 30u,            ///< 30 ms between steps.
    .accelSteps  = pctToCbus(2),   ///< ~2 % per step in both directions.
    .brakeSteps  = pctToCbus(2),   ///< Symmetric centering rate.
    .neutralBand = 0u,             ///< No extra dead-band (upstream input device handles it).
};


// EOF simulation_presets.h
