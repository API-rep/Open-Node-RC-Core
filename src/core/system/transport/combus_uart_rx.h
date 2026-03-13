/******************************************************************************
 * @file combus_uart_rx.h
 * Generic ComBus UART receiver.
 *
 * @details Receives binary ComBus frames from a HardwareSerial port,
 * validates SOF, length, and CRC-8/MAXIM, then exposes the latest
 * valid snapshot via combus_uart_rx_snapshot().
 *
 * The caller provides pre-allocated analog and digital backing arrays
 * at init time — the same pattern used by combus_frame_decode().
 * Ring-buffer framing with SOF-scan re-sync on corruption or garbage.
 *
 * Typical integration:
 * @code
 *   // Caller-owned backing storage:
 *   static uint16_t s_analog[N_ANALOG];
 *   static bool     s_digital[N_DIGITAL];
 *
 *   // In setup():
 *   combus_uart_rx_init(&Serial2, BAUD, RX_PIN, TX_PIN,
 *                       s_analog, N_ANALOG,
 *                       s_digital, N_DIGITAL);
 *
 *   // In loop():
 *   combus_uart_rx_update();
 *   const ComBusFrame* snap = combus_uart_rx_snapshot();
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

#include <core/combus/combus_frame.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the combus UART receiver.
 *
 * @param serial          HardwareSerial port (e.g. &Serial2).
 * @param baud            UART baud rate.
 * @param rxPin           GPIO RX pin.
 * @param txPin           GPIO TX pin (-1 for RX-only link).
 * @param analogBuf       Caller-allocated array, receives analog values.
 * @param analogBufSize   Capacity of analogBuf (number of uint16_t entries).
 * @param digitalBuf      Caller-allocated array, receives digital values.
 * @param digitalBufSize  Capacity of digitalBuf (number of bool entries).
 */
void combus_uart_rx_init( HardwareSerial* serial,
                          uint32_t        baud,
                          int             rxPin,
                          int             txPin,
                          uint16_t*       analogBuf,
                          uint8_t         analogBufSize,
                          bool*           digitalBuf,
                          uint8_t         digitalBufSize );



/**
 * @brief Poll UART and decode incoming frames — call every loop iteration.
 *
 * @details Non-blocking. Drains available bytes into a ring buffer and
 * attempts to decode complete frames. May process multiple back-to-back
 * frames per call. Updates the internal snapshot on each valid frame.
 */
void combus_uart_rx_update();



/**
 * @brief Return a pointer to the latest valid decoded snapshot.
 *
 * @details Returns nullptr until at least one valid frame has been received.
 * The pointer is stable until the next valid frame overwrites the snapshot.
 *
 * @return Const pointer to the latest ComBusFrame, or nullptr.
 */
const ComBusFrame* combus_uart_rx_snapshot();



/**
 * @brief Milliseconds since the last valid frame was received.
 *
 * @return Age in ms, or UINT32_MAX if no frame has ever been received.
 */
uint32_t combus_uart_rx_age_ms();



/**
 * @brief True if a valid frame was received within the last timeoutMs ms.
 */
bool combus_uart_rx_is_alive(uint32_t timeoutMs = 500u);

// EOF combus_uart_rx.h
