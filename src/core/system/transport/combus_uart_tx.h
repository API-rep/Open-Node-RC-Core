/******************************************************************************
 * @file combus_uart_tx.h
 * Generic ComBus UART transmitter.
 *
 * @details Serializes the live ComBus into a binary frame and sends it
 * over a HardwareSerial port at a configurable rate. Timer-gated,
 * non-blocking — safe to call from any main loop.
 *
 * Platform dependency: HardwareSerial (Arduino ESP32). The codec itself
 * (combus_frame_encode) is platform-independent.
 *
 * Typical integration:
 * @code
 *   // In init:
 *   combus_uart_tx_init(&Serial2, envId,
 *                       N_ANALOG, N_DIGITAL,
 *                       BAUD, TX_PIN, RX_PIN, TX_HZ);
 *
 *   // In loop:
 *   combus_uart_tx_update(&comBus, failsafeActive);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <Arduino.h>

#include <struct/combus_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the combus UART transmitter.
 *
 * @param serial    HardwareSerial port to use (e.g. &Serial2).
 * @param envId     Machine-type identifier embedded in each frame header.
 * @param nAnalog   Number of analog channels to encode per frame.
 * @param nDigital  Number of digital channels to encode per frame.
 * @param baud      UART baud rate.
 * @param txPin     ESP32 GPIO TX pin (connects to remote RX).
 * @param rxPin     ESP32 GPIO RX pin (-1 for TX-only link).
 * @param txHz      Frame transmit rate in Hz.
 */
void combus_uart_tx_init( HardwareSerial* serial,
                           uint8_t         envId,
                           uint8_t         nAnalog,
                           uint8_t         nDigital,
                           uint32_t        baud,
                           int             txPin,
                           int             rxPin,
                           uint32_t        txHz );



/**
 * @brief combus UART transmit update
 *
 * @details Non-blocking routine call from main loop. Sends one frame 
 * every (1000 / txHz) ms. Returns immediately if it is not yet time
 * to transmit or if init was not called.
 *
 * @param bus       ComBus instance to serialize. Must not be nullptr.
 * @param failSafe  True when the caller-side failsafe is active.
 */
void combus_uart_tx_update(const ComBus* bus, bool failSafe = false);

// EOF combus_uart_tx.h
