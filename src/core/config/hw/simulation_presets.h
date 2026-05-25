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

#include <struct/simulation_struct.h>                          // GearShiftProfile, GearStepCfg, SubGearStepCfg
#include <struct/combus/processors/motion/cb_ramp_struct.h>    // CbRampCfg
#include <core/system/combus/combus_res.h>  // CbusMaxVal, CbusNeutral, pctToCbus


// =============================================================================
// 3. VIRTUAL GEARBOX PRESETS  (GearShiftProfile — speed-threshold shift FSM)
// =============================================================================

/**
 * @brief Virtual 3-speed gearbox preset — heavy wheeled construction vehicle.
 *
 * @details Calibrated against the CAT 3408 operating band (~650–2100 RPM).
 *   Suitable for ADTs and heavy haulers paired with the VIRTUAL_3SPEED gearbox
 *   type.  Assign via `kDumperTruckGearShift` in `dumper_truck_motion.h`.
 *
 *   Callers convert their speed signal to simulated RPM before calling
 *   `gear_fsm_update()`:
 *     `rpm = |currentPos − CbusNeutral| × maxRpm / CbusNeutral`
 *   where `maxRpm = gear[gearCount-1].upShift` = 2100.
 *
 *   gear[0].upShift    =  650 — shift 1→2 at ~31 % speed
 *   gear[1].upShift    = 1300 — shift 2→3 at ~62 % speed
 *   gear[1].shiftDelta =  250 — RPM drop on 1→2 shift (land ~400 RPM in G2)
 *   gear[2].shiftDelta =  400 — RPM drop on 2→3 shift (land ~900 RPM in G3)
 *   gear[1].downShift  =  250 — shift 3→2 coasting when RPM < 250
 *   gear[1].downShiftBraking = 400 — shift 3→2 braking when RPM < 400
 *
 *   shiftGuardMs       = 2000 — 2 s minimum between consecutive shifts
 *   shiftDelta values are initial estimates — tune on hardware.
 */
static constexpr GearStepCfg kHeavy3_steps[] = {
    //  upShift  downShift  downShiftBraking  rampTime  shiftDelta
    {      650,          0,                0,        30,          0 },  // gear 1 — no upshift into it
    {     1300,        250,              400,        50,        250 },  // gear 2 — RPM drops 250 on 1→2
    {     2100,        650,              800,        70,        400 },  // gear 3 — RPM drops 400 on 2→3
};

///< Sub-gear ramp times (ms) and RPM ceilings.
///< Index 0 = slowest crawl, index 2 = fastest crawl.
///< All ramp times are slower than normal gear-1 (30 ms).
///< maxPct ceiling: full throttle in that sub-gear maps to `gear[0].upShift × maxPct / 100`.
static constexpr SubGearStepCfg kHeavy3_subSteps[] = {
    //  rampTime  maxPct
    {  500,   28 },  // sub-1: ~28 % of 650 RPM = ~182 RPM  (slow crawl)
    {  200,   62 },  // sub-2: ~62 % of 650 RPM = ~403 RPM  (medium crawl)
    {   80,   89 },  // sub-3: ~89 % of 650 RPM = ~579 RPM  (barely slower than G1)
};

static constexpr GearShiftProfile kGearShift_Heavy3Speed {
    .gearCount        = uint8_t(std::size(kHeavy3_steps)),
    .gear             = kHeavy3_steps,
    .shiftGuardMs     = 2000u,
    .subGearCount     = uint8_t(std::size(kHeavy3_subSteps)),
    .subGear          = kHeavy3_subSteps,
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
