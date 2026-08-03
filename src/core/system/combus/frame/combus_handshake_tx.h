/******************************************************************************
 * @file  combus_handshake_tx.h
 * @brief TX side of the ComBus handshake frame dispatcher.
 *
 * @details Mirror of combus_tx.{h,cpp} — split to keep each side of the
 *   handshake in its own translation unit.  See combus_handshake.h for the
 *   full design contract (payload layout, scope, etc.).
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <core/system/combus/frame/combus_handshake.h> // umbrella (constants, logBootWarning, formatMd5Hex)
#include <core/system/hw/node_com.h>                  // NodeCom


// =============================================================================
// TX ENTRY POINT
// =============================================================================

/**
 * @brief Build and transmit ONE handshake frame (seq=0) carrying the
 *        locally-generated MD5 + project version.
 *
 * @details Manual one-shot — there is no automatic burst.  Intended for
 *   debug commands, test fixtures, or future user-triggered handshake.
 *
 *   Compile-time `-D COMBUS_MD5_CHECK_DISABLE` does NOT affect this sender
 *   — the local MD5 + version are always sent verbatim, even if the local
 *   RX path is configured to bypass the compare.  Keeps the wire payload
 *   self-consistent: what you send IS what you expect.
 *
 * @param[in] nodeCom  Transport interface to write through (must be
 *                     non-null with a non-null `write` callback).
 *
 * @return Number of bytes written on the wire, 0 on guard-fail
 *         (null transport, etc.).
 */
uint8_t combus_handshake_sendOnce( NodeCom* nodeCom );

// EOF combus_handshake_tx.h