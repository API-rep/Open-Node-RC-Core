/******************************************************************************
 * @file sound_uart_tx.h
 * Sound module — machine-side UART transmitter.
 *
 * @details Serializes the active ComBus into a compact binary frame and
 * sends it over a HardwareSerial port at a configurable frequency.
 * Called from the machine's main loop — non-blocking (timer-gated).
 *
 * Typical integration in machines/main.cpp loop():
 * @code
 *   sound_uart_tx_update(comBus, failsafeActive);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the UART transmitter for the sound module.
 *
 * @details Configures the HardwareSerial port at SOUND_UART_BAUD,
 * using the pins defined in the active board configuration.
 * Must be called once during machine_init() after combus is ready.
 *
 * @param txPin  ESP32 GPIO pin for UART TX (connects to sound ESP32 RX).
 * @param rxPin  ESP32 GPIO pin for UART RX (-1 if not used / TX-only link).
 */
void sound_uart_tx_init(int txPin, int rxPin = -1);

/**
 * @brief Periodic transmit update — call from main loop.
 *
 * @details Sends one frame every (1000 / SOUND_TRANSPORT_TX_HZ) ms.
 * Non-blocking: returns immediately if it is not yet time to transmit.
 *
 * @param bus       Current ComBus state to serialize.
 * @param failSafe  True when the machine-side failsafe is active.
 */
void sound_uart_tx_update(const ComBus& bus, bool failSafe = false);

// EOF sound_uart_tx.h
