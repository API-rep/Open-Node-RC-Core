/******************************************************************************
 * @file  dumper_truck_sound.cpp
 * @brief Sound engine dynamic profile — dumper truck class (definition).
 *
 * @details Values migrated from CaboverCAT3408.h (rc_engine_sound legacy
 *          format).  To tune the behaviour, edit the kVehicleSoundDynamics
 *          initialiser below.  See the header for field semantics and the
 *          speed-scale reference (0 = standstill, 500 = max speed).
 *****************************************************************************/

#include "dumper_truck_sound.h"
#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>  // kDumperTruckGearShift — single source of truth for shift thresholds


// =============================================================================
// 1. PROFILE DEFINITION
// =============================================================================

const VehicleSoundProfile kVehicleSoundDynamics = {
    .engineMode          = EngineMode::ENGINE_HYDRAULIC,
    .gearboxType         = GearboxType::VIRTUAL_3SPEED,
    .engineAcc           = 2,              // 1 = loco slow, 9 = trophy fast
    .engineDec           = 1,
    .clutchEngagingPoint = 80u,
    .maxRpmPercentage    = 350u,
    .gearShift        = kDumperTruckGearShift,

    // --- ESC inertia ramp ---
    // Machine node already applies inertia (sim_ramp ~1.7 s).  Set esc() ramp
    // to near-instant so the engine sound tracks ESC_RPM_BUS directly, same as the
    // motors.  escRampTime=1 ms + max steps → escPulseWidth snaps to pw3 in
    // one call; the remaining sound inertia is the machine's sim_ramp only.
    .escRampTime             = {  1u,  1u,  1u },  // near-instant, all gears
    .escBrakeSteps           = 255u,              // full range in one step (uint8_t max)
    .escAccelerationSteps    = 255u,              // full range in one step (uint8_t max)
    .maxClutchSlippingRpm    = 250u,
    .automaticReverseAccelPct = 100u,
    .lowRangePct             = 58u,
    .numberOfAutoGears       = 3u,
    .shiftingAutoThrottle    = true,

    // --- Throttle/RPM dependent volume floors ---
    .engineIdleVolumePct      = 60u,
    .engineRevVolumePct       = 60u,
    .fullThrottleVolumePct    = 140u,
    .dieselKnockIdleVolumePct = 0u,
    .dieselKnockStartPoint    = 110u,
    .jakeBrakeIdleVolumePct   = 0u,
    .turboIdleVolumePct       = 0u,
    .fanStartPoint            = 0u,
    .fanIdleVolumePct         = 0u,
    .chargerStartPoint        = 10u,
    .chargerIdleVolumePct     = 10u,
    .wastegateIdleVolumePct   = 0u,
};


// EOF dumper_truck_sound.cpp
