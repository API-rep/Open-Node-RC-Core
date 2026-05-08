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


// =============================================================================
// 1. TYPES
// =============================================================================

/** @brief Vehicle engine mode — controls throttle mapping and clutch behaviour. */
enum class EngineMode : uint8_t {
    NORMAL,      ///< Standard bidirectional throttle; forward and reverse.
    LOADER,      ///< Forward throttle + hydraulic RPM boost (dump body / loader arm).
    EXCAVATOR,   ///< Forward-only throttle; hydraulic clutch; RPM lowering from hydraulicLoad.
    TRACKED,     ///< Dual-stick throttle (max of L/R tracks); forced gear-2.
    AIRPLANE,    ///< Forward-only with 10 % deadband; no ESC ramp control.
    STEAM_LOCO,  ///< targetRpm tracks currentSpeed; forced gear-2.
};

/** @brief Gearbox simulation type — controls RPM calculation and gear-selection strategy. */
enum class GearboxType : uint8_t {
    REAL_3SPEED,    ///< Physical 3-position switch on CH2; real gear ratios.
    VIRTUAL_3SPEED, ///< Virtual gear ratios + speed-threshold auto-shifting (SEMI_AUTOMATIC + VIRTUAL_3_SPEED).
    VIRTUAL_16SEQ,  ///< 16-speed sequential via up/down impulses on CH2 (VIRTUAL_16_SPEED_SEQUENTIAL).
};

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
