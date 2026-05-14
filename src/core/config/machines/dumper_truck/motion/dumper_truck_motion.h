/******************************************************************************
 * @file  dumper_truck_motion.h
 * @brief Motion preset aliases — dumper truck class.
 *
 * @details Exposes two preset aliases for all dumper-truck builds:
 *
 *   `kDumperTruckTractionPreset` — pointer to `kTraction_Heavy`.
 *   `kDumperTruckGearShift`      — pointer to `kGearShift_Heavy3Speed`.
 *
 *   Both presets are defined in `motion_presets.h` and shared with any
 *   other vehicle class that may use the same dynamics.
 *****************************************************************************/
#pragma once

#include <core/config/hw/motion_presets.h>  // kTraction_Heavy, kGearShift_Heavy3Speed


// =============================================================================
// 1. TRACTION PRESET ALIAS
// =============================================================================

/// @brief Canonical traction preset for the dumper-truck / articulated hauler class.
static constexpr const MotionConfig* kDumperTruckTractionPreset = &kTraction_Heavy;


// =============================================================================
// 2. VIRTUAL-GEARBOX PRESET ALIAS
// =============================================================================

/// @brief Canonical gear-shift preset for the dumper-truck / articulated hauler class.
static constexpr const GearShiftProfile* kDumperTruckGearShift = &kGearShift_Heavy3Speed;


// EOF dumper_truck_motion.h
