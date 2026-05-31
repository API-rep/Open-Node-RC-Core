/******************************************************************************
 * @file  dumper_truck_motion.h
 * @brief Motion preset aliases — dumper truck class.
 *
 * @details Exposes the gear-shift preset alias for all dumper-truck builds.
 *   `kDumperTruckGearShift` — pointer to `kGearShift_Heavy6Speed`.
 *   Traction preset is now handled by the SimDev pipeline (sim_traction).
 *****************************************************************************/
#pragma once

#include <core/config/hw/simulation_presets.h>  // kGearShift_Heavy6Speed


// =============================================================================
// 1. VIRTUAL-GEARBOX PRESET ALIAS
// =============================================================================

/// @brief Canonical gear-shift preset for the dumper-truck / articulated hauler class.
static constexpr const GearShiftProfile* kDumperTruckGearShift = &kGearShift_Heavy6Speed;


// EOF dumper_truck_motion.h

