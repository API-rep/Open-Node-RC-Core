/******************************************************************************
 * @file  combus_handshake_rx.h
 * @brief ComBus handshake, RX side: decode a frame, update the contract of the link.
 *
 * @details Constants and per-link context: combus_handshake.h.
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <core/system/combus/protocol/frame/combus_handshake.h> // constants, CombusHandshakeContext


/**
 * @brief Try to decode one handshake frame from the RX ring buffer, and apply it.
 *
 * @details Called by the RX path when the frame at the head of the ring has
 *   `seq == 0`.  A frame with a bad CRC loses its first byte only, so the RX path
 *   re-synchronises.  A valid frame is consumed; it may arm an ACK, change the
 *   next / stable mode, make the contract STABLE or drop it back to UNKNOWN.
 *
 * @param      ctx          Per-link handshake context.
 * @param      ringBuf      Ring buffer base pointer.
 * @param      ringBufSize  Ring buffer capacity.
 * @param[in,out] ringHead  Current read index.
 * @param[in,out] ringCount Bytes currently in the ring buffer.
 *
 * @return Number of bytes consumed; 0 if no complete valid frame was found
 *   (incomplete: retry on the next poll).
 */

uint8_t combus_handshake_tryDecode(
	CombusHandshakeContext* ctx,
	uint8_t*                ringBuf,
	uint8_t                 ringBufSize,
	uint8_t&                ringHead,
	uint8_t&                ringCount );


/**
 * @brief Compare an on-wire MD5 and major version against a local MD5.
 *
 * @details Silent: the RX path logs the changes of the contract.  Only the major
 *   version breaks a contract.  Exposed so the tests can exercise the compare
 *   without a ring buffer.
 *
 * @warning A null `localMd5` is a configuration error (link not initialised
 *   through combus_protocol_init()): the system halts, it is never replaced
 *   silently.
 *
 * @param[in] localMd5   The MD5 this link expects (16 bytes), typically
 *                       `ctx->expected.stable`.
 * @param[in] wireMd5    The MD5 received on the wire (16 bytes).
 * @param[in] wireMajor  The project version MAJOR received on the wire.
 *
 * @return true if the MD5 and the major version match, or always with the
 *   COMBUS_MD5_CHECK_DISABLE bring-up bypass.
 */

bool combus_handshake_compare(
	const uint8_t* localMd5,
	const uint8_t* wireMd5,
	uint8_t        wireMajor );

// EOF combus_handshake_rx.h
