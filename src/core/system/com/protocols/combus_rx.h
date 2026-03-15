/******************************************************************************
 * @file combus_rx.h
 * @brief ComBus receiver module
 *
 * @details Receives binary ComBus frames from any NodeCom* transport interface
 *   provided at init time. Validates SOF, length, and CRC-8/MAXIM, then
 *   exposes the latest valid snapshot via `combus_rx_snapshot()`.
 *
 *   The caller provides pre-allocated analog and digital output buffers at init
 *   time. The module writes decoded values directly into them on each valid frame.
 *   Incoming bytes are accumulated in an internal ring buffer.
 *   A SOF-scan re-synchronizes framing on corruption or line garbage.
 *
 * Typical integration:
 * @code
 *    // Caller-owned backing storage:
 *   static uint16_t analog[N_ANALOG];
 *   static bool     digital[N_DIGITAL];
 *
 *    // In setup():
 *   NodeCom* com = uart_com_init(&Serial2, BAUD, RX_PIN, TX_PIN, "sound_rx");
 *   constexpr ComBusFrameCfg cfg = { MACHINE_TYPE, N_ANALOG, N_DIGITAL };
 *   combus_rx_init(com, cfg, analog, digital);
 *
 *    // In loop():
 *   combus_rx_update();
 *   const ComBusFrame* snap = combus_rx_snapshot();
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../node_com.h"
#include <core/combus/combus_frame.h>
#include <struct/outputs_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus receiver.
 *
 * @param nodeCom    NodeCom transport interface (from *_com_init).
 * @param frameCfg   Combus frame config (buffer sizes, envID)
 * @param analogBuf  Caller-allocated analog buffer array
 * @param digitalBuf Caller-allocated digital buffer array
 */
void combus_rx_init( NodeCom*            nodeCom,
                     ComBusFrameCfg      frameCfg,
                     uint16_t*           analogBuf,
                     bool*               digitalBuf );



/**
 * @brief Check input buffer and decode incoming frames.
 *
 * @details Drains available bytes into a ring buffer and attempts
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
