/******************************************************************************
 * @file sound_config.h
 * Sound module — ComBus channel mapping configuration.
 *
 * @details Maps Open Node RC Core ComBus channels to their corresponding
 * logical roles in the rc_engine_sound HAL. One config file per machine
 * type — select via MACHINE_TYPE build flag.
 *
 * Analog ComBus → pulseWidth conversion formula (sound engine uses µs):
 *   pulseWidth = map(cbValue, 0, CB_MAX, PULSE_MIN_US, PULSE_MAX_US)
 *   where PULSE_MIN_US = 1000, PULSE_MAX_US = 2000, center = 1500
 *
 * Transport parameters — tune to add/remove channels from the UART frame.
 * Keeping the bus minimal is important: this same frame will be reused for
 * ESP-Now RF communication between machines and remotes.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <core/config/combus/combus_types.h>    // AnalogComBusID, DigitalComBusID (IS_MACHINE absent → enums only)


// =============================================================================
// 1. TRANSPORT FRAME PARAMETERS
// =============================================================================

/// Machine type identifier embedded in every frame header.
/// Derived from MACHINE_TYPE (CombusLayout enum, core_defs.h) — must be defined
/// in the platformio env. Example: -D MACHINE_TYPE=DUMPER_TRUCK (value = 1).
#define SOUND_TRANSPORT_ENV_ID  static_cast<uint8_t>(CombusLayout::MACHINE_TYPE)

/// Number of analog ComBus channels included in the transport frame.
/// Keep this ≤ total AnalogComBusID::CH_COUNT to avoid out-of-bounds.
#define SOUND_TRANSPORT_N_ANALOG    static_cast<uint8_t>(AnalogComBusID::CH_COUNT)

/// Number of digital ComBus channels included in the transport frame.
#define SOUND_TRANSPORT_N_DIGITAL   static_cast<uint8_t>(DigitalComBusID::CH_COUNT)

/// UART transmit rate in Hz (frames per second, machine → sound ESP32).
#define SOUND_TRANSPORT_TX_HZ       50u

/// UART baud rate — must match on both machine and sound ESP32.
#define SOUND_UART_BAUD             115200u


// =============================================================================
// 2. ANALOG CHANNEL ROLES  (ComBus → sound engine)
// =============================================================================

/// ComBus analog channel → rc_engine_sound THROTTLE input.
/// Maps full range [0, 65535] → pulseWidth [1000, 2000] µs.
#define SOUND_CB_THROTTLE   AnalogComBusID::DRIVE_SPEED_BUS

/// ComBus analog channel → rc_engine_sound STEERING input.
/// Used by the sound engine for indicator auto-detection and tyre squeal.
#define SOUND_CB_STEERING   AnalogComBusID::STEERING_BUS

/// ComBus analog channel → rc_engine_sound GEARBOX / left-stick input.
/// For vehicles without a dedicated gearbox, this drives DUMP or is unused.
#define SOUND_CB_GEARBOX    AnalogComBusID::DUMP_BUS


// =============================================================================
// 3. DIGITAL CHANNEL ROLES  (ComBus → sound engine)
// =============================================================================

/// ComBus digital channel → rc_engine_sound HORN / siren trigger.
#define SOUND_CB_HORN           DigitalComBusID::HORN

/// ComBus digital channel → rc_engine_sound FUNCTION_R (lights, jake brake…).
#define SOUND_CB_LIGHTS         DigitalComBusID::LIGHTS

/// ComBus digital channel → rc_engine_sound engine on/off (KEY).
/// This is the authoritative source for keyOn — the transport flags byte
/// carries only transport-level status (failSafe); application state like
/// keyOn is transmitted as a normal digital channel.
#define SOUND_CB_KEY            DigitalComBusID::KEY

/// ComBus digital channel → left turn indicator.
/// Encodes into pulseWidth[FUNCTION_L] at 1100 µs when active (silent when HAZARDS is also set).
#define SOUND_CB_INDICATOR_L    DigitalComBusID::INDICATOR_LEFT

/// ComBus digital channel → right turn indicator.
/// Encodes into pulseWidth[FUNCTION_L] at 1900 µs when active (silent when HAZARDS is also set).
#define SOUND_CB_INDICATOR_R    DigitalComBusID::INDICATOR_RIGHT

/// ComBus digital channel → hazard lights (both indicators simultaneously).
/// When active, pulseWidth[FUNCTION_L] is held at center (1500 µs) — indicator
/// tick-tack sound is suppressed. Light flashing is handled by the machine output code.
#define SOUND_CB_HAZARDS        DigitalComBusID::HAZARDS


// =============================================================================
// 4. PULSE WIDTH RANGE  (µs, rc_engine_sound convention)
// =============================================================================

#define SOUND_PULSE_MIN_US  1000u   ///< Minimum valid pulseWidth (full back / off)
#define SOUND_PULSE_MAX_US  2000u   ///< Maximum valid pulseWidth (full forward / on)
#define SOUND_PULSE_CTR_US  1500u   ///< Center / neutral pulseWidth

// EOF sound_config.h
