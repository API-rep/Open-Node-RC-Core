/******************************************************************************
 * @file  combus_handshake_tx.cpp
 * @brief ComBus handshake, TX side: boot burst, periodic requests and ACK.
 *****************************************************************************/

#include "combus_handshake_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


	// Build one handshake frame (announced MD5, version, `flags` as given) and write it.
	// A NEXT handshake carries the MD5 of the next set.
static uint8_t sendHandshake( const CombusHandshakeContext* ctx, NodeCom* nodeCom, uint8_t flags )
{
	const uint8_t* stableMd5 = ctx->expected.stable;
	const uint8_t* nextMd5   = ctx->expected.next;
		// Through a coupler, this end announces its own side.
	if (ctx->announced.stable) { stableMd5 = ctx->announced.stable; }
	if (ctx->announced.next)   { nextMd5   = ctx->announced.next; }
	const uint8_t* md5ToSend = ((flags & kCombusHandshakeFlagNext) && nextMd5)
		? nextMd5
		: stableMd5;

	uint8_t frame[ sizeof(CombusFrameSof)
	             + sizeof(CombusFrameHeader)
	             + kCombusHandshakePayloadLen
	             + sizeof(uint8_t) ];
	const uint8_t payloadStart = sizeof(CombusFrameSof)
	                          + sizeof(CombusFrameHeader);

	frame[0] = CombusFrameSof;
	frame[sizeof(CombusFrameSof) + 0u] = 0u;     // nAnalog
	frame[sizeof(CombusFrameSof) + 1u] = 0u;     // nDigital
	frame[sizeof(CombusFrameSof) + 2u] = 0u;     // seq = 0: reserved to the handshake
	frame[sizeof(CombusFrameSof) + 3u] = flags;  // ACK and / or NEXT bits, 0 = legacy request

	for (uint8_t i = 0u; i < 16u; ++i) {
		frame[payloadStart + i] = md5ToSend[i];
	}
	frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
	frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;
	frame[payloadStart + 18u] = combus::wire::kProjectVersionDev;

	const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;
	frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);
	const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

	nodeCom->write(nodeCom->ctx, frame, totalLen);
	return totalLen;
}


	// Kind of the next request: while UNKNOWN, NEXT as long as NEXT tries are left;
	// once STABLE, the kind of the mode.
static uint8_t requestFlags( CombusHandshakeContext* ctx )
{
	if (ctx->contractValidated) { return ctx->nextMode ? kCombusHandshakeFlagNext : 0u; }
	if (ctx->nextTriesLeft > 0u) {
		ctx->nextTriesLeft--;
		return kCombusHandshakeFlagNext;
	}
	return 0u;
}


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
	if (ctx->burstActive) {
		if (ctx->lastBurstMs == 0u
			|| (uint32_t)(nowMs - ctx->lastBurstMs) >= kCombusHandshakeBurstPeriodMs) {
			written = (uint8_t)(written + sendHandshake(ctx, nodeCom, requestFlags(ctx)));
			ctx->lastBurstMs   = nowMs;
			ctx->lastRequestMs = nowMs;
			if (ctx->burstRemaining > 0u) { ctx->burstRemaining--; }
			if (ctx->burstRemaining == 0u) { ctx->burstActive = false; }   // burst complete
		}
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
