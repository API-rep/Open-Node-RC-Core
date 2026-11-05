/******************************************************************************
 * @file  combus_handshake_rx.cpp
 * @brief ComBus handshake, RX side: decode a frame, compare its MD5 and major
 *        version, update the contract of the link.
 *****************************************************************************/

#include "combus_handshake_rx.h"

#include <string.h>
#include <stddef.h>
#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>



// =============================================================================
// 1. COMPARE AND INTERNAL HELPERS
// =============================================================================

/**
 * @brief Ring reader. Gives the byte at a logical position, wrapping around the end.
 *
 * @details Local on purpose: the handshake owns its decoder, with no dependency on the
 *   internals of combus_rx.  The sum is taken on an int, before the modulo: the capacity
 *   of the ring is not a power of two, a uint8_t wrap would give a wrong index.
 *
 * @param ringBuf      Ring buffer base pointer.
 * @param ringBufSize  Ring buffer capacity, not 0.
 * @param ringHead     Index of the oldest byte.
 * @param i            Logical position, 0 = oldest.
 *
 * @return The byte at that position.
 */

static uint8_t ringByteAt(const uint8_t* ringBuf,
                          uint8_t        ringBufSize,
                          uint8_t        ringHead,
                          uint8_t        i)
{
	return ringBuf[(ringHead + i) % ringBufSize];
}


/**
 * @brief Contract check of a handshake. Says whether the peer speaks what this end expects.
 *
 * @details In order:
 *   1. Bring-up bypass: every handshake is accepted (banner logged at boot).
 *   2. A null local MD5 is a config error: it halts rather than accept or refuse on
 *      a contract nobody declared.
 *   3. The 16 MD5 bytes and the major version must be equal.  The minor and dev versions
 *      are only logged, by the caller.
 */

bool combus_handshake_compare(
	const uint8_t* localMd5,
	const uint8_t* wireMd5,
	uint8_t        wireMajor )
{
		// 1. Bring-up bypass
	if (COMBUS_MD5_CHECK_DISABLE) { return true; }

		// 2. Config error, never replaced silently
	if (!localMd5) {
		sys_log_err(
			"[COMBUS_HANDSHAKE] FATAL: CombusHandshakeContext::expected.stable is null "
			"— link was not properly initialized via combus_link_start() "
			"— system halted\n");
		while (1) { /* halt */ }
	}

		// 3. MD5 and major version
	return (memcmp(wireMd5, localMd5, 16) == 0)
		&& (wireMajor == combus::wire::kProjectVersionMajor);
}


/**
 * @brief Refusal log. Shows both MD5 and versions, once until the contract is STABLE again.
 *
 * @details A peer that disagrees sends a handshake every period: without the one-shot
 *   guard, it would flood the serial log.
 *
 * @param ctx        Per-link handshake context.
 * @param wireMd5    The MD5 received (16 bytes).
 * @param wireMajor  The project version MAJOR received.
 * @param wireMinor  The project version MINOR received.
 */

static void logRefusal( CombusHandshakeContext* ctx, const uint8_t* wireMd5,
                        uint8_t wireMajor, uint8_t wireMinor )
{
		// Check the one-shot guard, then set it
	if (ctx->refusalLogged) { return; }

	ctx->refusalLogged = true;

		// Both MD5, as text
	char localMd5Hex[33];
	char wireMd5Hex[33];

	combus_handshake_formatMd5Hex(ctx->expected.stable, localMd5Hex);
	combus_handshake_formatMd5Hex(wireMd5, wireMd5Hex);

		// Log both sides
	sys_log_warn(
		"[COMBUS_HANDSHAKE] link '%s' REFUSED  local md5=%s ver=%u.%u  wire md5=%s ver=%u.%u\n",
		ctx->name ? ctx->name : "?",
		localMd5Hex,
		(unsigned)combus::wire::kProjectVersionMajor,
		(unsigned)combus::wire::kProjectVersionMinor,
		wireMd5Hex, (unsigned)wireMajor, (unsigned)wireMinor);
}



// =============================================================================
// 2. DECODE
// =============================================================================

/**
 * @brief Handshake reader. Takes one frame from the ring and updates the contract of the link.
 *
 * @details In order:
 *   1. Guards, then the boot banner, logged on the first attempt.
 *   2. Complete frame required, otherwise retry on the next poll.  The length is fixed.
 *   3. Copy to a linear buffer: the frame may cross the end of the ring.
 *   4. CRC.  On a mismatch, only the first byte is dropped: the next frame may start
 *      inside these bytes.
 *   5. Valid frame: consumed, and noted as seen.
 *   6. Flags, MD5 and version read from the linear copy.
 *   7. Request on a bidirectional end: an ACK of its kind is armed, matching or not,
 *      so a refused peer still gets this end's MD5 and can log the disagreement.
 *   8. NEXT handshake: it decides the mode and never refuses the contract.
 *   9. Legacy handshake: a refusal drops the contract back to UNKNOWN.
 *   10. Only an UNKNOWN contract that was accepted gets here: it becomes STABLE.
 *
 *   Steps 8 and 9 agree on what proves the contract: a bidirectional end takes the ACK
 *   of its own request (the peer does receive it), a receive-only end takes the request.
 */

uint8_t combus_handshake_tryDecode(
	CombusHandshakeContext* ctx,
	uint8_t*                ringBuf,
	uint8_t                 ringBufSize,
	uint8_t&                ringHead,
	uint8_t&                ringCount )
{
		// 1. Guards, boot banner
	if (!ctx || !ringBuf || ringBufSize == 0u) { return 0u; }
	if (ringCount == 0u)                       { return 0u; }

	combus_handshake_logBootWarningIfNeeded(ctx);

		// 2. A complete frame is required: otherwise retry on the next poll
	const uint8_t expectedLen = CombusFrameHandshakeMinLen;

	if (ringCount < expectedLen) {
		return 0u;
	}

		// 3. Copy the candidate to a linear buffer, for the CRC
	uint8_t linear[CombusFrameHandshakeMinLen];

		// Read through the wrap of the ring
	for (uint8_t i = 0u; i < expectedLen; ++i) {
		linear[i] = ringByteAt(ringBuf, ringBufSize, ringHead, i);
	}

		// 4. Validate the CRC: on a mismatch, drop one byte and re-synchronise
	uint8_t crcExpected = linear[expectedLen - 1u];
	uint8_t crcActual   = combus_frame_crc8(linear, (uint8_t)(expectedLen - 1u));

		// Check the CRC: the RX path looks for the next SOF from the following byte
	if (crcActual != crcExpected) {
		ringHead  = (uint8_t)((ringHead + 1u) % ringBufSize);
		ringCount = (uint8_t)(ringCount - 1u);

		sys_log_info("[COMBUS_HANDSHAKE] CRC mismatch — dropped 1B, re-sync\n");

		return 0u;
	}

		// 5. Valid frame: consume it
	ringHead  = (uint8_t)((ringHead + expectedLen) % ringBufSize);
	ringCount = (uint8_t)(ringCount - expectedLen);

	combus_handshake_internal::markEverReceived(ctx);

		// 6. Read the frame: request or ACK, legacy or NEXT, MD5 and version
	const uint8_t  flags         = linear[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, flags)];
	const bool     isAck         = (flags & kCombusHandshakeFlagAck)  != 0u;
	const bool     isNext        = (flags & kCombusHandshakeFlagNext) != 0u;
	const uint8_t  payloadOffset = sizeof(CombusFrameSof)
	                             + sizeof(CombusFrameHeader);

		// Fields of the payload
	const uint8_t* wireMd5   = &linear[payloadOffset];
	const uint8_t  wireMajor = linear[payloadOffset + 16u];
	const uint8_t  wireMinor = linear[payloadOffset + 17u];
	const uint8_t  wireDev   = linear[payloadOffset + 18u];

		// 7. A bidirectional end answers every request with an ACK of its kind, matching or not
	if (!isAck && ctx->bidirectional) {
			// Set the ACK of the same kind
		if (isNext) { ctx->ackPendingNext = true; }
		else        { ctx->ackPending     = true; }
	}

		// 8. NEXT handshake: it only decides the mode, never refuses the contract
	if (isNext) {
		const bool nextMatch = ctx->hasCandidates
		                    && combus_handshake_compare(ctx->expected.next, wireMd5, wireMajor);

			// 8.1 Another next set: back to stable mode; while UNKNOWN, no more NEXT requests
		if (!nextMatch) {
				// Leave next mode, logged once
			if (ctx->nextMode) {
				ctx->nextMode = false;

				sys_log_info("[COMBUS_HANDSHAKE] link '%s' mode stable (the peer announces another next set)\n",
				             ctx->name ? ctx->name : "?");
			}

				// An ACK proves the peer has another next set: stop asking
			if (isAck && !ctx->contractValidated) { ctx->nextTriesLeft = 0u; }   // legacy at once

			return expectedLen;
		}

			// Refresh the ACK window
		if (isAck) { ctx->lastAckMs = millis(); }

			// 8.2 STABLE: the mode follows what the peer announces, the two ends meet in one period
		if (ctx->contractValidated) {
				// Enter next mode, logged once
			if (!ctx->nextMode) {
				ctx->nextMode = true;

				sys_log_info("[COMBUS_HANDSHAKE] link '%s' mode next (the peer announces the same next set)\n",
				             ctx->name ? ctx->name : "?");
			}

			return expectedLen;
		}

			// 8.3 UNKNOWN: a bidirectional end needs the ACK of its own request
		if (ctx->bidirectional && !isAck) { return expectedLen; }

			// Set the mode of the contract to come
		ctx->nextMode = true;
	} else {

			// 9. Legacy handshake: refused, the contract is (back to) UNKNOWN
		if (!combus_handshake_compare(ctx->expected.stable, wireMd5, wireMajor)) {
			logRefusal(ctx, wireMd5, wireMajor, wireMinor);
			combus_handshake_internal::dropContract(ctx, "handshake refused");

			return expectedLen;
		}

			// Accepted: a bidirectional end needs the ACK of its own request (proof
			// that the peer receives it); a receive-only end takes the request.
		if (ctx->bidirectional && !isAck) { return expectedLen; }

			// Refresh the ACK window
		if (isAck) { ctx->lastAckMs = millis(); }

			// Check for a contract already STABLE: nothing else changes
		if (ctx->contractValidated) { return expectedLen; }

			// Set the mode of the contract to come
		ctx->nextMode = false;
	}

		// 10. The contract becomes STABLE, in the mode chosen above
	combus_handshake_internal::markContractValidated(ctx);
	combus_handshake_internal::stopBurst(ctx);

		// Set the state of a new contract
	ctx->lastAckMs       = millis();     // the ACK window starts now
	ctx->dataSinceStable = false;
	ctx->refusalLogged   = false;

		// Log the change, with the versions received
	sys_log_info("[COMBUS_HANDSHAKE] link '%s' contract STABLE  mode %s  wire ver=%u.%u dev=%u%s\n",
	             ctx->name ? ctx->name : "?",
	             ctx->nextMode ? "next" : (ctx->hasCandidates ? "stable" : "legacy (no candidate)"),
	             (unsigned)wireMajor, (unsigned)wireMinor, (unsigned)wireDev,
	             (wireMinor != combus::wire::kProjectVersionMinor || wireDev != combus::wire::kProjectVersionDev)
	                 ? "  (minor or dev version differs)" : "");

	return expectedLen;
}

// EOF combus_handshake_rx.cpp
