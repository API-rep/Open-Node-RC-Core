/******************************************************************************
 * @file  dumper_truck_motion.h
 * @brief Traction motion preset alias — dumper truck class.
 *
 * @details Aliases the shared kTraction_Heavy preset as the canonical
 *          traction config for this vehicle class.  Machine-specific motor
 *          definitions (e.g. volvo_A60H_bruder.cpp) may still reference
 *          kTraction_Heavy directly; this alias exists as a stable per-class
 *          symbol for future vehicle profiles that might need to deviate.
 *****************************************************************************/
#pragma once

#include <core/config/hw/motion_presets.h>  // MotionConfig, kTraction_Heavy


// =============================================================================
// 1. PRESET ALIAS
// =============================================================================

/// @brief Canonical traction preset for the dumper-truck / articulated hauler class.
static constexpr const MotionConfig* kDumperTruckTractionPreset = &kTraction_Heavy;


// EOF dumper_truck_motion.h
