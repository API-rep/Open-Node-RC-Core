/******************************************************************************
 * @file  loader_sound.cpp
 * @brief Sound engine dynamic profile — wheel loader class (definition).
 *
 * @details Values migrated from VolvoL120H.h (rc_engine_sound legacy format).
 *          To tune the behaviour, edit the kVehicleSoundDynamics initialiser
 *          below.  See the header for field semantics and the speed-scale
 *          reference (0 = standstill, 500 = max speed).
 *
 *          The loader uses VIRTUAL_3SPEED with upShift thresholds set to 500
 *          (maximum) to replicate the original 1-gear automatic behaviour
 *          (NumberOfAutomaticGears = 1): selectedGear never advances beyond 1.
 *****************************************************************************/

#include "loader_sound.h"


// =============================================================================
// 1. PROFILE DEFINITION
// =============================================================================

// Guard: replace #if 0 with #if MACHINE == <loader machine id> when a real
// wheel loader machine is added (winter 2026).
#if 0
const VehicleSoundProfile kVehicleSoundDynamics = {
    .engineMode          = EngineMode::ENGINE_HYDRAULIC,
    .gearboxType         = GearboxType::VIRTUAL_3SPEED, // 1-gear auto: upShift set to max
    .engineAcc           = 6,              // fast throttle response (wheel loader)
    .engineDec           = 3,
    .clutchEngagingPoint = 500u,           // CEP = max: hydraulic torque converter
    .maxRpmPercentage    = 200u,
    .upShift             = { 500, 500, 500 }, // 1-gear mode — never shifts to gear 2
    .downShift           = {   0,   0,   0 },
    .downShiftBraking    = {   0,   0,   0 },
};
#endif  // loader machine guard


// EOF loader_sound.cpp
