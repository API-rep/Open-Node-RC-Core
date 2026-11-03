/******************************************************************************
 * @file  combus_handshake_tx.cpp
 * @brief ComBus handshake, TX side: boot burst, periodic requests and ACK.
 *****************************************************************************/

#include "combus_handshake_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>


/**
 * @brief Handshake writer. Builds one frame and hands it to the transport.
 *
 * @details In order:
 *   1. MD5 to announce: `expected` by default; through a coupler, `announced` (this end
 *      announces its own side, not what it expects).  A NEXT handshake carries the next
 *      set, a legacy one the stable set.
 *   2. Header: no channel, `seq == 0` (reserved to the handshake), `flags` as given.
 *   3. Payload: the MD5 and the three version numbers.
 *   4. CRC over everything before it, then the write.
 *
 * @param ctx      Per-link handshake context; `expected.stable` is set (see the caller).
 * @param nodeCom  Transport to write through; `write` is set (see the caller).
 * @param flags    Kind of the frame: kCombusHandshakeFlagAck and / or
 *                 kCombusHandshakeFlagNext, 0 = legacy request.
 *
 * @return Number of bytes written, the size of a handshake frame.
 */

static uint8_t sendHandshake( const CombusHandshakeContext* ctx, NodeCom* nodeCom, uint8_t flags )
{
		// 1. MD5 to announce
	const uint8_t* stableMd5 = ctx->expected.stable;
	const uint8_t* nextMd5   = ctx->expected.next;

		// Through a coupler, this end announces its own side.
	if (ctx->announced.stable) { stableMd5 = ctx->announced.stable; }
	if (ctx->announced.next)   { nextMd5   = ctx->announced.next; }

		// A NEXT handshake carries the next set
	const uint8_t* md5ToSend = ((flags & kCombusHandshakeFlagNext) && nextMd5)
		? nextMd5
		: stableMd5;

		// 2. Header
	uint8_t frame[ sizeof(CombusFrameSof)
	             + sizeof(CombusFrameHeader)
	             + kCombusHandshakePayloadLen
	             + sizeof(uint8_t) ];
	const uint8_t payloadStart = sizeof(CombusFrameSof)
	                          + sizeof(CombusFrameHeader);

		// Set the fixed fields: no channel on this frame
	frame[0] = CombusFrameSof;
	frame[sizeof(CombusFrameSof) + 0u] = 0u;     // nAnalog
	frame[sizeof(CombusFrameSof) + 1u] = 0u;     // nDigital
	frame[sizeof(CombusFrameSof) + 2u] = 0u;     // seq = 0: reserved to the handshake
	frame[sizeof(CombusFrameSof) + 3u] = flags;  // ACK and / or NEXT bits, 0 = legacy request

		// 3. Payload

		// Copy the MD5
	for (uint8_t i = 0u; i < 16u; ++i) {
		frame[payloadStart + i] = md5ToSend[i];
	}

		// Set the version numbers
	frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
	frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;
	frame[payloadStart + 18u] = combus::wire::kProjectVersionDev;

		// 4. CRC, then the write
	const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;

	frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);

		// Send it
	const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

	nodeCom->write(nodeCom->ctx, frame, totalLen);

	return totalLen;
}


/**
 * @brief Kind of the next request. Tries the next set first, then settles on the mode.
 *
 * @details While UNKNOWN: NEXT as long as NEXT tries are left, each call using one, then
 *   legacy.  Once STABLE: the kind of the mode, with no try used.
 *
 * @param ctx  Per-link handshake context.
 *
 * @return kCombusHandshakeFlagNext, or 0 for a legacy request.
 */

static uint8_t requestFlags( CombusHandshakeContext* ctx )
{
		// STABLE: the kind of the mode
	if (ctx->contractValidated) { return ctx->nextMode ? kCombusHandshakeFlagNext : 0u; }

		// UNKNOWN: use up the NEXT tries
	if (ctx->nextTriesLeft > 0u) {
		ctx->nextTriesLeft--;

		return kCombusHandshakeFlagNext;
	}

		// No try left: legacy
	return 0u;
}


/**
 * @brief Handshake emission of a link. Decides, on every poll, what must leave now.
 *
 * @details In order:
 *   1. Guards: nothing leaves on a link not initialised (no transport, no MD5).
 *   2. ACK: what the RX side armed on a request.  One of each kind at most; never an
 *      ACK to an ACK.
 *   3. Boot burst, if active: closed as soon as the contract is STABLE, otherwise one
 *      request per kCombusHandshakeBurstPeriodMs, the first one at once.  The periodic
 *      requests wait for its end.  Compiled out with COMBUS_HANDSHAKE_BURST_DISABLE.
 *   4. Then one request per kCombusHandshakePeriodMs, whatever the state of the contract.
 *
 *   The timestamps are compared by unsigned difference: millis() may wrap around.
 */

uint8_t combus_handshake_tx_update( CombusHandshakeContext* ctx,
                                    NodeCom*                nodeCom )
{
		// 1. Guard checks
	if (!ctx || !nodeCom || !nodeCom->write || !ctx->expected.stable) { return 0u; }

	const uint32_t nowMs   = millis();
	uint8_t        written = 0u;

		// 2. A request was received: answer it with an ACK of its kind (never answer an ACK)
	if (ctx->ackPending) {
		ctx->ackPending = false;
		written = (uint8_t)(written + sendHandshake(ctx, nodeCom, kCombusHandshakeFlagAck));
	}

		// Same for a NEXT request
	if (ctx->ackPendingNext) {
		ctx->ackPendingNext = false;
		written = (uint8_t)(written + sendHandshake(ctx, nodeCom,
		                                            kCombusHandshakeFlagAck | kCombusHandshakeFlagNext));
	}

		// 3. Boot burst, stopped once the contract is STABLE
#if !COMBUS_HANDSHAKE_BURST_DISABLE
	if (ctx->burstActive && ctx->contractValidated) {
		ctx->burstActive    = false;
		ctx->burstRemaining = 0u;
	}

		// Check for a burst in progress
	if (ctx->burstActive) {
			// First frame at once, then one per burst period
		if (ctx->lastBurstMs == 0u
			|| (uint32_t)(nowMs - ctx->lastBurstMs) >= kCombusHandshakeBurstPeriodMs) {
			written = (uint8_t)(written + sendHandshake(ctx, nodeCom, requestFlags(ctx)));

				// Set the timers: the periodic requests count from here
			ctx->lastBurstMs   = nowMs;
			ctx->lastRequestMs = nowMs;

				// Count this frame: the last one closes the burst
			if (ctx->burstRemaining > 0u) { ctx->burstRemaining--; }
			if (ctx->burstRemaining == 0u) { ctx->burstActive = false; }   // burst complete
		}

			// No periodic request during the burst
		return written;
	}
#endif

		// 4. Then one request per period, whatever the state of the contract
	if ((uint32_t)(nowMs - ctx->lastRequestMs) >= kCombusHandshakePeriodMs) {
		written = (uint8_t)(written + sendHandshake(ctx, nodeCom, requestFlags(ctx)));
		ctx->lastRequestMs = nowMs;
	}

	return written;
}

// EOF combus_handshake_tx.cpp
