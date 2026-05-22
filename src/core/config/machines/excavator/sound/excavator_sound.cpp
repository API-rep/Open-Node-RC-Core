/******************************************************************************
 * @file  excavator_sound.cpp
 * @brief Sound engine dynamic profile — excavator class (definition).
 *
 * @details Values migrated from Caterpillar323Excavator.h (rc_engine_sound
 *          legacy format).  To tune the behaviour, edit the kVehicleSoundDynamics
 *          initialiser below.  See the header for field semantics and the
 *          speed-scale reference (0 = standstill, 500 = max speed).
 *
 *          upShift / downShift / downShiftBraking thresholds are not consumed
 *          in EXCAVATOR mode (no traction-driven gear selection) — set to
 *          neutral values (max / zero).
 *****************************************************************************/

#include "excavator_sound.h"


// =============================================================================
// 1. PROFILE DEFINITION
// =============================================================================

// Guard: replace #if 0 with #if MACHINE == <excavator machine id> when a real
// excavator machine is added (winter 2026).
#if 0
const VehicleSoundProfile kVehicleSoundDynamics = {
    .engineMode          = EngineMode::EXCAVATOR,
    .gearboxType         = GearboxType::VIRTUAL_3SPEED, // unused in EXCAVATOR mode; stays in gear 1
    .engineAcc           = 2,              // 1 = loco slow, 9 = trophy fast
    .engineDec           = 1,
    .clutchEngagingPoint = 500u,           // CEP = max: virtual clutch never engages
    .maxRpmPercentage    = 200u,
    .gearShift           = nullptr,  // EXCAVATOR mode — no traction gearbox
};
#endif  // excavator machine guard


// EOF excavator_sound.cpp
