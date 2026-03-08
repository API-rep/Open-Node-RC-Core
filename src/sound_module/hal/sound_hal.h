/******************************************************************************
 * @file sound_hal.h
 * Sound module — ComBus → rc_engine_sound hardware abstraction layer.
 *
 * @details Replaces all RC-receiver reading functions of the original
 * rc_engine_sound project (readSbusCommands, readIbusCommands,
 * readPwmSignals, processRawChannels) with a single call that translates
 * the latest UART-received ComBus snapshot into the pulseWidth[] array
 * format expected by the sound engine.
 *
 * Pulse width convention (unchanged from rc_engine_sound):
 *   1000 µs = full reverse / off
 *   1500 µs = neutral / center
 *   2000 µs = full forward / on
 *
 * ComBus analog convention (Open Node RC Core):
 *   0       = full reverse / off
 *   32767   = neutral / center
 *   65535   = full forward / on
 *
 * Integration in the sound ESP32 main:
 * @code
 *   // In setup():
 *   sound_hal_init();
 *
 *   // In loop() (Task0 or Task1, before sound engine logic):
 *   sound_uart_rx_update();       // receive UART bytes
 *   sound_hal_update();           // snapshot → pulseWidth[] + failSafe
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>


// =============================================================================
// 1. CONSTANTS
// =============================================================================

/// Size of the pulseWidth array (rc_engine_sound convention: 14 = 13 ch + ch0).
#define SOUND_HAL_PULSE_ARRAY_SIZE  14u

/// Pulse width for a digital channel in "active / ON" state (µs).
#define SOUND_HAL_DIGITAL_ON_US     2000u

/// Pulse width for a digital channel in "inactive / OFF" state (µs).
#define SOUND_HAL_DIGITAL_OFF_US    1000u

/// Link loss timeout in ms before failsafe is raised.
#define SOUND_HAL_FAILSAFE_TIMEOUT_MS  500u


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the sound HAL.
 *
 * @details Fills pulseWidth[] with neutral values (1500 µs) and marks
 * failSafe active until the first valid ComBus frame arrives.
 * Also sets pulseZero[] to 1500 and pulseLimit to match
 * rc_engine_sound's expected calibration defaults.
 */
void sound_hal_init();

/**
 * @brief Update pulseWidth[] from the latest UART snapshot.
 *
 * @details Must be called every loop iteration after sound_uart_rx_update().
 * If no valid snapshot is available or the link is stale, sets failSafe = true
 * and holds pulseWidth[] at neutral.
 *
 * Updates the following rc_engine_sound global variables:
 *   - pulseWidth[THROTTLE]   ← analogBus[DRIVE_SPEED_BUS]
 *   - pulseWidth[STEERING]   ← analogBus[STEERING_BUS]
 *   - pulseWidth[GEARBOX]    ← analogBus[DUMP_BUS]
 *   - pulseWidth[HORN]       ← digitalBus[HORN]
 *   - pulseWidth[FUNCTION_R] ← digitalBus[LIGHTS]
 *   - failSafe               ← COMBUS_FLAG_FAILSAFE (transport flag) or link timeout
 */
void sound_hal_update();

/**
 * @brief Return the current failsafe state as seen by the HAL.
 *
 * @return true when the link is lost or the machine side raised failsafe.
 */
bool sound_hal_is_failsafe();

// EOF sound_hal.h
