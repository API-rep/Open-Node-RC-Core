/******************************************************************************
 * @file  dumper_truck_sound.h
 * @brief Sound engine dynamic profile — dumper truck class.
 *
 * @details Centralises all behavioural engine-sound parameters for the
 *          dumper-truck vehicle class (CAT 3408 / Volvo A60H family).
 *          These values were previously scattered across CaboverCAT3408.h.
 *
 *          Parameters that remain in the legacy vehicle file pending the
 *          rc_engine_sound removal (escRampTime*, escBrakeSteps,
 *          escAccelerationSteps) are documented in the class README.md.
 *
 *          All speed values use the rc_engine_sound internal scale:
 *            0 = standstill, 500 = maximum speed.
 *
 *          This file exposes `kVehicleSoundDynamics` — the canonical instance
 *          name included via `dumper_truck_config.h` (section 4 — SOUND_ENABLED guard)
 *          and consumed from main.cpp.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <core/config/hw/shaker_presets.h>
#include <struct/sound_struct.h>   ///< EngineMode, GearboxType


// =============================================================================
// 1. TYPES
// =============================================================================

// VehicleSoundProfile, EngineMode and GearboxType are defined in <struct/sound_struct.h>.


// =============================================================================
// 2. DECLARATIONS
// =============================================================================

/// @brief Default sound dynamics for the dumper-truck / articulated hauler class.
/// @details Canonical symbol — consumed from main.cpp as `kVehicleSoundDynamics`.
extern const VehicleSoundProfile kVehicleSoundDynamics;


// =============================================================================
// 3. SHAKER PRESET
// =============================================================================

/// @brief Active shaker preset for the dumper-truck / articulated hauler class.
static constexpr ShakerCfg kShakerCfg = kShaker_GtPowerStock;


// EOF dumper_truck_sound.h
