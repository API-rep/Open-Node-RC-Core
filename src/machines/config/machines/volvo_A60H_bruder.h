/*!****************************************************************************
 * @file    volvo_A60H_bruder.h
 * @brief   Volvo A60H Bruder — vehicle configuration header.
 *
 * @details Declares device-index enums and extern arrays for the Bruder
 *   Volvo A60H full-electric conversion.  Physical layout:
 *   - 6 DC motors for traction (one per wheel, cabin + trailer axles).
 *   - 1 DC motor for steering (one or two actuators wired in parallel).
 *   - 1 DC motor for the dump body (two actuators wired in parallel).
 *   - No servo-driven devices on this build.
 *   - 6 signal sources: horn, lights, engine key, two indicators, hazards.
 *
 *   Array contents are defined in `volvo_A60H_bruder.cpp`.
 *   The `machine` constexpr aggregates all pointers for use by init code.
 *******************************************************************************
 */
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include <core/config/combus/combus_types.h>


// =============================================================================
// Build-time parameters
// =============================================================================

#ifndef BOARD
  #define BOARD            ESP32_8M_6S  ///< Default motherboard if not set by build flags.
#endif

#define MAX_FW_SPEED         100.00     ///< Maximum forward speed cap (0–100 %).
#define MAX_BACK_SPEED       100.00     ///< Maximum reverse speed cap (0–100 %).

#define M_DEF_PWM_FREQ        16000     ///< Default DC-motor PWM frequency (Hz).
#define SRV_DEF_PWM_FREQ         50     ///< Default servo PWM frequency (Hz).
#define COOLING_FAN_SPEED       100     ///< Cooling fan duty cycle (%).

  // Include board pin/peripheral definitions.
#include <machines/config/boards/boards.h>


// =============================================================================
// Device index enums
// =============================================================================

/**
 * @brief Indices into `dcDevArray[]`.
 *
 * @details `DC_DRV_COUNT` is used as the array size and loop bound — do not
 *   assign an explicit value to any enumerator after it.
 */
enum DrvDev {
    STEERING = 0,               ///< Steering actuator (1 or 2 wired in //).
    CABIN_LEFT_MOTOR,           ///< Front-left traction motor (cabin axle, left).
    CABIN_RIGHT_MOTOR,          ///< Front-right traction motor (cabin axle, right).
    TRAILER_FRONT_LEFT_MOTOR,   ///< Middle-left traction motor (trailer front axle).
    TRAILER_FRONT_RIGHT_MOTOR,  ///< Middle-right traction motor (trailer front axle).
    TRAILER_REAR_LEFT_MOTOR,    ///< Rear-left traction motor (trailer rear axle).
    TRAILER_REAR_RIGHT_MOTOR,   ///< Rear-right traction motor (trailer rear axle).
    DUMP_ACTUATOR,              ///< Dump-body actuator (2 actuators wired in //).
    DC_DRV_COUNT                ///< Sentinel — number of DC-motor devices.
};

/// DC-motor device table. Data defined in `volvo_A60H_bruder.cpp`.
extern DcDevice dcDevArray[DC_DRV_COUNT];


/**
 * @brief Indices into `SrvDevArray[]`.
 *
 * @note No servo-driven devices on this build. `SRV_COUNT = 0` prevents
 *   zero-size array issues in the device control loop.
 */
enum SrvDev {
    // (no servo devices currently)
    SRV_COUNT  ///< Sentinel — number of servo devices (0 on this build).
};

/// Servo device table. Data defined in `volvo_A60H_bruder.cpp`.
extern SrvDevice SrvDevArray[];


/**
 * @brief Indices into `sigDevArray[]`.
 *
 * @details Signal devices carry a ComBus channel reference and a `DevUsage`
 *   tag that lets the sound and output modules derive their behaviour without
 *   additional mapping tables.  Entries tagged `UNDEFINED` are handled by
 *   internal FSM logic (engine key) or a multiplexer (indicators/hazards)
 *   inside the relevant module.
 */
enum SigDev {
    HORN_SIG = 0,   ///< Horn trigger → DevUsage::SIG_HORN.
    LIGHTS_SIG,     ///< Main lights toggle → DevUsage::SIG_LIGHT.
    KEY_SIG,        ///< Engine on/off key → DevUsage::UNDEFINED (handled by FSM).
    INDIC_L_SIG,    ///< Left indicator → DevUsage::UNDEFINED (mux in sound_core).
    INDIC_R_SIG,    ///< Right indicator → DevUsage::UNDEFINED (mux in sound_core).
    HAZARDS_SIG,    ///< Hazard flashers → DevUsage::UNDEFINED (mux in sound_core).
    SIG_COUNT       ///< Sentinel — number of signal devices.
};

/// Signal device table. Data defined in `volvo_A60H_bruder.cpp`.
extern SigDevice sigDevArray[SIG_COUNT];


// =============================================================================
// Vehicle config aggregate
// =============================================================================

/**
 * @brief Top-level machine descriptor for the Volvo A60H Bruder.
 *
 * @details Aggregates all device array pointers and counts.  Consumed by
 *   `machine_init()` and the sound / output module init functions.
 */
inline constexpr Machine machine {
  .infoName      = "Volvo A60H Bruder",
  .combusLayout  = CombusLayout::DUMPER_TRUCK,
  .dcDev         = dcDevArray,
  .dcDevCount    = DC_DRV_COUNT,
  .srvDev        = SrvDevArray,
  .srvDevCount   = SRV_COUNT,
  .sigDev        = sigDevArray,
  .sigDevCount   = SIG_COUNT
};

// EOF volvo_A60H_bruder.h