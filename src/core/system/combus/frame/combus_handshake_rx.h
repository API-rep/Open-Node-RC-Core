/******************************************************************************
 * @file  combus_handshake_rx.h
 * @brief RX side of the ComBus handshake frame dispatcher.
 *
 * @details Mirror of combus_rx.{h,cpp} — split to keep each side of the
 *   handshake in its own translation unit.  Both sides share the constants
 *   and the boot-warning helper exposed by combus_handshake.h.
 *
 *   See combus_handshake.h for the full design contract (payload layout,
 *   scope, etc.).
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <core/system/combus/frame/combus_handshake.h> // umbrella (constants, logBootWarning, formatMd5Hex)

// Compile-time bypass flag — default OFF, override with `-D COMBUS_MD5_CHECK_DISABLE=1`.
#ifndef COMBUS_MD5_CHECK_DISABLE
  #define COMBUS_MD5_CHECK_DISABLE  0
#endif


// =============================================================================
// RX ENTRY POINT
// =============================================================================

/**
 * @brief Attempt to decode a complete handshake frame from the RX ring buffer.
 *
 * @details See combus_handshake.h for the full contract — same signature,
 *   same return semantics, same CRC behaviour.  This declaration lives
 *   separately from combus_handshake.h so consumers that only need RX
 *   can include just this header (matters when TX is stubbed out).
 */
uint8_t combus_handshake_tryDecode(
    uint8_t*       ringBuf,
    uint8_t        ringBufSize,
    uint8_t&       ringHead,
    uint8_t&       ringCount );


// =============================================================================
// RX HELPERS — exposed for unit tests
// =============================================================================

/**
 * @brief Compare an on-wire MD5 + version pair against the locally-generated
 *        copy, log the result, and short-circuit when COMBUS_MD5_CHECK_DISABLE
 *        is active.  Pulled out of combus_handshake.cpp so unit tests can
 *        exercise the compare path without an RX ring buffer.
 *
 * @param[in] wireMd5    Pointer to 16 bytes received on the wire.
 * @param[in] wireMajor  On-wire project version MAJOR.
 * @param[in] wireMinor  On-wire project version MINOR.
 *
 * @return `true` iff the contract was effectively validated (real
 *         MD5+version match).  `false` for mismatch AND for the
 *         COMBUS_MD5_CHECK_DISABLE bypass path — bypass is a debug
 *         switch and is NOT considered a validated contract.
 */
bool combus_handshake_compareAndLog(
    const uint8_t* wireMd5,
    uint8_t        wireMajor,
    uint8_t        wireMinor );


// EOF combus_handshake_rx.h