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


// =============================================================================
// 1. PROFILE DEFINITION
// =============================================================================

const DumperTruckSoundProfile kVehicleSoundDynamics = {
    .engineAcc           = 2,              // 1 = loco slow, 9 = trophy fast
    .engineDec           = 1,
    .clutchEngagingPoint = 80u,
    .maxRpmPercentage    = 350u,
    .upShift             = { 150, 300, 500 },  // gear 1→2, 2→3, guard
    .downShift           = {   0,  60, 160 },  // 2→1, 3→2 coasting, guard
    .downShiftBraking    = {   0,  90, 210 },  // 2→1, 3→2 braking,  guard
};


// EOF dumper_truck_sound.cpp
