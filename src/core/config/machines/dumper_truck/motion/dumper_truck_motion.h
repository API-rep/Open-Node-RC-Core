/******************************************************************************
 * @file  dumper_truck_motion.h
 * @brief Motion preset aliases — dumper truck class.
 *
 * @details Exposes the gear-shift preset alias for all dumper-truck builds.
 *   `kDumperTruckGearShift` — pointer to `kGearShift_VolvoD16J`.
 *   Traction preset is now handled by the SimDev pipeline (sim_traction).
 *****************************************************************************/
#pragma once

#include <core/config/hw/simulation_presets.h>  // kGearShift_VolvoD16J


// =============================================================================
// 1. VIRTUAL-GEARBOX PRESET ALIAS
// =============================================================================

/// @brief Canonical gear-shift preset for the dumper-truck / articulated hauler class.
static constexpr const GearShiftProfile* kDumperTruckGearShift = &kGearShift_VolvoD16J;


// EOF dumper_truck_motion.h

