/******************************************************************************
 * @file combus_tx.h
 * @brief ComBus transmitter module
 *
 * @details Serializes the live ComBus into a binary frame and sends it via
 * any NodeCom* physical transport interface (UART, ESP-Now, …) provided at
 * init time. Timer-gated and non-blocking: the update function does nothing
 * if the transmit interval has not elapsed since the last frame.
 *
 * Typical integration:
 * @code
 *   // In init:
 *   NodeCom* com = uart_com_init(&Serial2, BAUD, TX_PIN, RX_PIN, "combus");
 *   constexpr ComBusFrameCfg cfg = { N_ANALOG, N_DIGITAL };
 *   combus_tx_init(com, cfg, TX_HZ);

 *
 *   // In loop:
 *   combus_tx_update(&comBus, failsafeActive);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/frame/combus_frame_defs.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus transmitter.
 *
 * @param nodeCom   Claimed transport interface (from *_com_init).
 * @param frameCfg  ComBus layout descriptor (nAnalog, nDigital).
 * @param txHz      Frame transmit rate in Hz.

 */

void combus_tx_init( NodeCom*        nodeCom,
                     ComBusFrameCfg  frameCfg,
                     uint32_t        txHz );



/**
 * @brief Encode and transmit one ComBus frame if the period has elapsed.
 *
 * @details Timer-gated and non-blocking. Does nothing if the transmit interval
 *   has not elapsed since the last frame.
 *
 * @param bus         Live ComBus to encode.
 * @param failSafe    Set true to flag the frame as failsafe-active.
 */
void combus_tx_update( const ComBus* bus, bool failSafe );



// =============================================================================
// 2. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

// Forward declaration — the full definition lives in combus_handshake.h.
struct CombusHandshakeContext;

/**
 * @brief Wire the per-link handshake context to the TX module.
 *
 * @details P3 — must be called BEFORE `combus_tx_init()` so the burst
 *   is armed on init.  Typically called from `combus_protocol_init()`
 *   with the same context as `combus_rx_init()` (shared between TX
 *   and RX of the same link).
 *
 *   Multiple independent ComBus interfaces may coexist; each link has
 *   its own context.  See CombusHandshakeContext in combus_handshake.h.
 *
 * @param ctx  Per-link handshake context (may be null to disable burst).
 */
void combus_tx_set_handshake_ctx( CombusHandshakeContext* ctx );

// EOF combus_tx.h

