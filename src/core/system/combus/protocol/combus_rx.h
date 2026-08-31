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
 *   constexpr ComBusFrameCfg cfg = { N_ANALOG, N_DIGITAL };
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

#include <core/system/hw/node_com.h>
#include <core/system/combus/frame/combus_frame.h>
#include <core/system/combus/frame/combus_frame_defs.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus receiver.
 *
 * @param nodeCom    NodeCom transport interface (from *_com_init).
 * @param frameCfg   Combus frame config (buffer sizes: nAnalog, nDigital)
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



// =============================================================================
// 2. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

// Forward declaration — the full definition lives in combus_handshake.h.
struct CombusHandshakeContext;

/**
 * @brief Wire the per-link handshake context to the RX module.
 *
 * @details P3 — must be called BEFORE `combus_rx_init()` so the
 *   contract-validated flag is shared with the TX module of the same
 *   link.  Typically called from `combus_protocol_init()` with the
 *   same context as `combus_tx_set_handshake_ctx()`.
 *
 *   Multiple independent ComBus interfaces may coexist; each link has
 *   its own context.  See CombusHandshakeContext in combus_handshake.h.
 *
 * @param ctx  Per-link handshake context (may be null).
 */
void combus_rx_set_handshake_ctx( CombusHandshakeContext* ctx );

// EOF combus_rx.h

