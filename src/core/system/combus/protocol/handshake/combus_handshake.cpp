/******************************************************************************
 * @file  combus_handshake.cpp
 * @brief ComBus handshake: shared helpers and changes of the per-link context.
 *
 * @details The RX and TX paths live in their own files (combus_handshake_rx.cpp,
 *   combus_handshake_tx.cpp).  The state lives in CombusHandshakeContext, one
 *   per link: this file holds none.
 *****************************************************************************/

#include "combus_handshake.h"

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


	// The generated header is the source of truth of the wire length.
static_assert(kCombusHandshakePayloadLen ==
                   combus::wire::kCombusHandshakeWirePayloadLen,
               "kCombusHandshakePayloadLen disagrees with "
               "combus::wire::kCombusHandshakeWirePayloadLen.");



// =============================================================================
// 1. SHARED HELPERS
// =============================================================================

/**
 * @brief MD5 printer. Gives every log the same form of an MD5, so they can be compared.
 *
 * @details Each byte gives two characters, high nibble first.
 */

void combus_handshake_formatMd5Hex(const uint8_t md5[16], char out[33])
{
	static constexpr char hex[] = "0123456789abcdef";

		// Two characters per byte, high nibble first
	for (uint8_t i = 0u; i < 16u; ++i) {
		out[2u * i    ] = hex[(md5[i] >> 4) & 0x0Fu];
		out[2u * i + 1u] = hex[ md5[i]       & 0x0Fu];
	}

		// Close the string
	out[32u] = '\0';
}


/**
 * @brief Boot banner of a link. Says in the log whether its MD5 compare is active.
 *
 * @details Invoked from the first decode attempt rather than from a global constructor
 *   (fragile init order on Arduino).  In order:
 *   1. Guards: already logged, or link not initialised (no expected MD5).
 *   2. Local MD5, as text.
 *   3. Banner.  With the compare disabled, it uses repeated markers and uppercase so a
 *      `grep` on a saved serial log reveals the bypass at once.
 *   4. One-shot guard set, for the following attempts.
 */

void combus_handshake_logBootWarningIfNeeded(CombusHandshakeContext* ctx)
{
		// 1. Guards
	if (!ctx || ctx->bootWarningLogged || !ctx->expected.stable) { return; }

		// 2. Local MD5, as text
	char md5Hex[33];

	combus_handshake_formatMd5Hex(ctx->expected.stable, md5Hex);

		// 3. Banner
	if (COMBUS_MD5_CHECK_DISABLE) {
			// Bypass active: a banner impossible to miss
		sys_log_info(
			"\n[COMBUS_HANDSHAKE] ############################################\n"
			"[COMBUS_HANDSHAKE] ##  COMBUS_MD5_CHECK_DISABLE IS ACTIVE        ##\n"
			"[COMBUS_HANDSHAKE] ##  Handshake MD5 comparison is BYPASSED.    ##\n"
			"[COMBUS_HANDSHAKE] ##  Bring-up debug only — no mismatch detect.##\n"
			"[COMBUS_HANDSHAKE] ##  local md5=%s  version=%u.%u               ##\n"
			"[COMBUS_HANDSHAKE] ############################################\n",
			md5Hex,
			(unsigned)combus::wire::kProjectVersionMajor,
			(unsigned)combus::wire::kProjectVersionMinor);
	} else {
			// Normal case: one line
		sys_log_info(
			"[COMBUS_HANDSHAKE] md5-check ENABLED  local md5=%s  version=%u.%u\n",
			md5Hex,
			(unsigned)combus::wire::kProjectVersionMajor,
			(unsigned)combus::wire::kProjectVersionMinor);
	}

		// 4. One-shot guard
	ctx->bootWarningLogged = true;
}



// =============================================================================
// 2. STATUS
// =============================================================================

	/// Contract state of a link; UNKNOWN without a context.
bool combus_handshake_is_contract_validated(const CombusHandshakeContext* ctx) {
	return ctx ? ctx->contractValidated : false;
}



// =============================================================================
// 3. INTERNAL BRIDGE
// =============================================================================

namespace combus_handshake_internal {

		/// A valid frame was seen, matching or not: the witness of the tests.
	void markEverReceived(CombusHandshakeContext* ctx) {
		if (ctx) { ctx->everReceived = true; }
	}


		/// Contract STABLE.  The RX path sets the rest (mode, ACK window) around this call.
	void markContractValidated(CombusHandshakeContext* ctx) {
		if (ctx) { ctx->contractValidated = true; }
	}


		/// NEXT requests to send while UNKNOWN: the burst and the retries, none without candidates.
	static uint8_t nextTries(const CombusHandshakeContext* ctx) {
		return ctx->hasCandidates ? (uint8_t)(kCombusHandshakeBurstCount + kCombusHandshakeNextRetries) : 0u;
	}


	/**
	 * @brief Fresh RX session. Forgets everything the previous session knew of the peer.
	 *
	 * @details Unlike dropContract(), also clears the pending ACKs and the refusal guard,
	 *   and stays silent: nothing was lost, the transport just (re)starts.
	 */

	void clearContractValidated(CombusHandshakeContext* ctx) {
			// Check the context
		if (!ctx) { return; }

			// Reset all that is known of the peer
		ctx->contractValidated = false;
		ctx->ackPending        = false;
		ctx->ackPendingNext    = false;
		ctx->dataSinceStable   = false;
		ctx->refusalLogged     = false;
		ctx->nextMode          = false;
		ctx->nextTriesLeft     = nextTries(ctx);
	}


	/**
	 * @brief Loss of a STABLE contract. Sends the link back to UNKNOWN once, with its reason.
	 *
	 * @details A no-op when already UNKNOWN, so a peer refused on every handshake logs one
	 *   line and does not re-arm the NEXT requests each time.  A pending ACK stays armed:
	 *   the peer asked for it, and the refusal guard stays set until the next STABLE.
	 */

	void dropContract(CombusHandshakeContext* ctx, const char* reason) {
			// Check the context: only a STABLE contract can be dropped
		if (!ctx || !ctx->contractValidated) { return; }

			// Back to UNKNOWN, NEXT requests re-armed
		ctx->contractValidated = false;
		ctx->dataSinceStable   = false;
		ctx->nextMode          = false;
		ctx->nextTriesLeft     = nextTries(ctx);

			// Log why
		sys_log_warn("[COMBUS_HANDSHAKE] link '%s' contract UNKNOWN (%s)\n",
		             ctx->name ? ctx->name : "?", reason);
	}


		/// Burst armed: both timestamps at 0, the TX side sends its first frame on its next update.
	void startBurst(CombusHandshakeContext* ctx) {
			// Check the context
		if (!ctx) { return; }

			// Set the burst
		ctx->burstActive    = true;
		ctx->burstRemaining = kCombusHandshakeBurstCount;
		ctx->lastBurstMs    = 0u;  // 0: first emission on the next update
		ctx->lastRequestMs  = 0u;  // the periodic requests count from the end of the burst
	}


		/// Burst over: the TX side goes on with its periodic requests.
	void stopBurst(CombusHandshakeContext* ctx) {
			// Check the context
		if (!ctx) { return; }

			// Clear the burst
		ctx->burstActive    = false;
		ctx->burstRemaining = 0u;
	}
}

// EOF combus_handshake.cpp
