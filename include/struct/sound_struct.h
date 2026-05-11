/*!****************************************************************************
 * @file  sound_struct.h
 * @brief Sound module channel descriptor.
 *
 * @details Defines `SoundChannel`, the static descriptor stored in the
 *   vehicle sound profile tables (`kSoundProfile[]`).  Each entry binds one
 *   ComBus channel index to an `ApplyFn` dispatch function; the profile table
 *   is declared per vehicle in
 *   `sound_module/config/profiles/<vehicle>/dumper_truck.h` and lives const
 *   in Flash.
 *******************************************************************************
 */
#pragma once

#include <stdint.h>
#include <defs/machines_defs.h>
#include <struct/combus_struct.h>


// =============================================================================
// 1. ENGINE BEHAVIOUR TYPES
// =============================================================================

/**
 * @brief Vehicle engine mode — controls throttle mapping and clutch behaviour.
 *
 * @details Named after the **capability set** of the powertrain, not the
 *   vehicle archetype.  Set once in `VehicleSoundProfile::engineMode`
 *   (or equivalent per-class profile); consumed at runtime by `mapThrottle()`,
 *   `engineMassSimulation()`, `gearboxDetection()`, and `esc()`.
 */
enum class EngineMode : uint8_t {
    ENGINE_ONLY,      ///< Traction only — bidirectional throttle, no hydraulics.
    ENGINE_HYDRAULIC, ///< Traction + hydraulic pump — RPM boosted by hydraulic demand (dump body, loader arm).
    EXCAVATOR,        ///< Hydraulic-primary — clutch always off, RPM lowered by hydraulicLoad, on/off via 3-pos switch.
    TRACKED,          ///< Dual-stick throttle (max of L/R tracks); forced gear-2.
    AIRPLANE,         ///< Forward-only with 10 % deadband; no ESC ramp control.
    STEAM_LOCO,       ///< targetRpm tracks currentSpeed; forced gear-2.
};

/**
 * @brief Gearbox simulation type — controls RPM calculation and gear-selection strategy.
 *
 * @details Set once in `VehicleSoundProfile::gearboxType`; consumed at runtime by
 *   `engineMassSimulation()`, `gearboxDetection()`, and `esc()`.
 */
enum class GearboxType : uint8_t {
    REAL_3SPEED,    ///< Physical 3-position switch on CH2; real gear ratios.
    VIRTUAL_3SPEED, ///< Virtual gear ratios + speed-threshold auto-shifting.
    VIRTUAL_16SEQ,  ///< 16-speed sequential via up/down impulses on CH2.
};


/**
 * @brief Engine dynamics tuning set — one instance per vehicle class.
 *
 * @details Stored in Flash as `kVehicleSoundDynamics` (defined in the
 *   machine-class `<vehicle>_sound.cpp`).  Consumed at runtime by
 *   `mapThrottle()`, `engineMassSimulation()`, `gearboxDetection()`
 *   and `esc()` in the sound module.
 *
 *   Array indices correspond to (selectedGear - 1).  Index 0 drives
 *   gear 1↔2 transitions; index 1 drives gear 2↔3; index 2 is an upper
 *   guard value.  Hysteresis: upShift[n] > downShift[n] — prevents gear
 *   hunting.  Fields `upShift`, `downShift`, and `downShiftBraking` are
 *   only consumed when `gearboxType == GearboxType::VIRTUAL_3SPEED`.
 */
struct VehicleSoundProfile {
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
// 2. SOUND CHANNEL
// =============================================================================

/**
 * @brief Dispatch function: extract one channel value and forward to SoundCore.
 *
 * @details Called once per ComBus update cycle for each `SoundChannel` entry.
 *   @p bus is the local ComBus instance; @p chanID is the channel index stored
 *   in the owning `SoundChannel`.  The function reads
 *   `bus->analogBus[chanID].value` or `bus->digitalBus[chanID].value` as
 *   appropriate and calls the corresponding `sound_core_set_*()` entry point.
 */
using ApplyFn = void (*)(const ComBus*, uint8_t);


/**
 * @brief Static descriptor for one ComBus channel → sound role binding.
 *
 * @details Entries are produced at compile time in the vehicle profile table
 *   (`kSoundProfile[]`).  The `usage` field carries the peripheral role for
 *   diagnostics and dashboard display; the interpreter calls
 *   `apply(snap, chanID)` without inspecting `usage`.
 *
 *   `isDigital` records the ComBus bus type and is intended for tooling and
 *   dashboard display.  The correct bus type is already encoded in `apply`.
 */
struct SoundChannel {
	DevUsage  usage;      ///< Peripheral role — for diagnostics and dashboard.
	uint8_t   chanID;     ///< ComBus channel index forwarded to apply().
	bool      isDigital;  ///< true = DigitalComBus channel, false = AnalogComBus.
	ApplyFn   apply;      ///< Dispatch fn — reads snap, calls sound_core_set_*().
};

// EOF sound_struct.h
