/******************************************************************************
 * @file combus_rx.h
 * @brief ComBus receiver — transport-agnostic.
 *
 * @details Receives binary ComBus frames from any TransportIface*, validates
 * SOF, length, and CRC-8/MAXIM, then exposes the latest valid snapshot via
 * combus_rx_snapshot().
 *
 * The caller provides pre-allocated analog and digital backing arrays at init
 * time — the same pattern used by combus_frame_decode(). Ring-buffer framing
 * with SOF-scan re-sync on corruption or garbage.
 *
 * Typical integration:
 * @code
 *   // Caller-owned backing storage:
 *   static uint16_t s_analog[N_ANALOG];
 *   static bool     s_digital[N_DIGITAL];
 *
 *   // In setup():
 *   TransportIface* t = uart_transport_init(&Serial2, BAUD, RX_PIN, TX_PIN, "sound_rx");
 *   combus_rx_init(t, s_analog, N_ANALOG, s_digital, N_DIGITAL);
 *
 *   // In loop():
 *   combus_rx_update();
 *   const ComBusFrame* snap = combus_rx_snapshot();
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "transport.h"
#include <core/combus/combus_frame.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus receiver.
 *
 * @param transport       Claimed transport interface (from uart_transport_init or similar).
 * @param analogBuf       Caller-allocated array, receives analog values.
 * @param analogBufSize   Capacity of analogBuf (number of uint16_t entries).
 * @param digitalBuf      Caller-allocated array, receives digital values.
 * @param digitalBufSize  Capacity of digitalBuf (number of bool entries).
 */
void combus_rx_init( TransportIface* transport,
                     uint16_t*       analogBuf,
                     uint8_t         analogBufSize,
                     bool*           digitalBuf,
                     uint8_t         digitalBufSize );


/**
 * @brief Poll transport and decode incoming frames — call every loop iteration.
 *
 * @details Non-blocking. Drains available bytes into a ring buffer and attempts
 * to decode complete frames. May process multiple back-to-back frames per call.
 * Updates the internal snapshot on each valid frame.
 */
void combus_rx_update();


/**
 * @brief Return a pointer to the latest valid decoded snapshot.
 *
 * @details Returns nullptr until at least one valid frame has been received.
 * The pointer is stable until the next valid frame overwrites the snapshot.
 *
 * @return Const pointer to the latest ComBusFrame, or nullptr.
 */
const ComBusFrame* combus_rx_snapshot();


/**
 * @brief Milliseconds since the last valid frame was received.
 *
 * @return Age in ms, or UINT32_MAX if no frame has ever been received.
 */
uint32_t combus_rx_age_ms();


/**
 * @brief True if a valid frame was received within the last timeoutMs ms.
 */
bool combus_rx_is_alive(uint32_t timeoutMs = 500u);

// EOF combus_rx.h
