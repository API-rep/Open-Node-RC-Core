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
 *          name included via `dumper_truck_config.h` (section 4 — SOUND_NODE guard)
 *          and consumed from main.cpp.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <core/config/hw/shaker_presets.h>
#include <struct/sound_struct.h>   ///< EngineMode, GearboxType


// =============================================================================
// 1. TYPES
// =============================================================================

// EngineMode and GearboxType enums are defined in <struct/sound_struct.h>.

/** @brief Engine dynamics tuning set for the dumper-truck vehicle class.
 *
 *  Array indices correspond to (selectedGear - 1).  Index 0 drives gear 1↔2
 *  transitions; index 1 drives gear 2↔3; index 2 is an upper guard value.
 *  Hysteresis: upShift[n] > downShift[n] for every n — prevents gear hunting.
 */
struct DumperTruckSoundProfile {
    EngineMode  engineMode;       ///< Vehicle engine / throttle behaviour mode.
    GearboxType gearboxType;      ///< Gearbox simulation type.
    int8_t   engineAcc;           ///< Engine mass accel step (scale 0..9, 1=loco, 9=trophy).
    int8_t   engineDec;           ///< Engine mass decel step (scale 0..9).
    uint16_t clutchEngagingPoint; ///< CEP — above this speed, RPM tracks ESC.
    uint32_t maxRpmPercentage;    ///< Max RPM as % of idle RPM.
    int16_t  upShift[3];          ///< Speed-threshold upshift points (spd 0..500, VIRTUAL_3SPEED only).
    int16_t  downShift[3];        ///< Downshift thresholds — coasting (VIRTUAL_3SPEED only).
    int16_t  downShiftBraking[3]; ///< Downshift thresholds — braking (VIRTUAL_3SPEED only).
};


// =============================================================================
// 2. DECLARATIONS
// =============================================================================

/// @brief Default sound dynamics for the dumper-truck / articulated hauler class.
/// @details Canonical symbol — exposed via sound_dynamics.h dispatcher as
///          `kVehicleSoundDynamics`.
extern const DumperTruckSoundProfile kVehicleSoundDynamics;


// =============================================================================
// 3. SHAKER PRESET
// =============================================================================

/// @brief Active shaker preset for the dumper-truck / articulated hauler class.
static constexpr ShakerCfg kShakerCfg = kShaker_GtPowerStock;


// EOF dumper_truck_sound.h
