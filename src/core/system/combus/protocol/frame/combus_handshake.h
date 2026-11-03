/******************************************************************************
 * @file  combus_handshake.h
 * @brief ComBus handshake: constants, per-link context and shared helpers.
 *
 * @details The handshake is the frame that proves two ends of a link speak the
 *   same contract.  It travels on the reserved `seq == 0` wire slot, apart from
 *   the control frames, and carries the MD5 of the link's contract plus the
 *   project version.  An end announces its MD5, the other compares it with the
 *   one it expects, and the contract of the link is STABLE or UNKNOWN.
 *
 *   Generic to every ComBus participant: node to node (machine, remote,
 *   coupler) or board to board inside a node.  The wire layout is described in
 *   combus_frame_protocol.md.
 *
 *   This umbrella holds what both sides share; each side has its own entry
 *   points:
 *     - combus_handshake_rx.h: combus_handshake_tryDecode()
 *     - combus_handshake_tx.h: combus_handshake_tx_update()
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/frame/combus_frame.h>   // CombusFrameHeader, CombusFrameSof
#include <core/system/hw/node_com.h>                          // NodeCom (TX path)

	// Generated MD5 of each view (namespace combus::wire), resolved per link by
	// combus_protocol_init().
#include <combus_md5.h>          // kCombusComBusMd5       (FULL view)
#include <combus_system_md5.h>   // kCombusSystemComBusMd5 (SYSTEM view)
#include <combus_base_md5.h>     // kCombusBaseComBusMd5   (BASE view)


	// The helpers of section 4 take a context, defined in section 5.
struct CombusHandshakeContext;



// =============================================================================
// 1. WIRE CONSTANTS
// =============================================================================

/**
 * @brief Fixed payload length of the handshake frame, in bytes.
 *
 * @details Excludes SOF, header and CRC-8.  Layout:
 * @code
 *   [0..15]  MD5 of the contract announced by this end (next set on a NEXT handshake)
 *   [16]     project version MAJOR: a difference breaks the contract
 *   [17]     project version MINOR: logged only
 *   [18]     project version DEV, the candidate pool: logged only
 * @endcode
 *
 *   The sender address does not ride here: it travels in the coupling frame
 *   (combus_coupling.h).
 *
 * @note Wire contract, also written in the generated header
 *   (combus::wire::kCombusHandshakeWirePayloadLen): a `static_assert` in
 *   combus_handshake.cpp keeps both equal.
 */

static constexpr uint8_t kCombusHandshakePayloadLen = 19u;


	/// Total handshake frame size on the wire (SOF + header + payload + CRC).
static constexpr uint8_t CombusFrameHandshakeMinLen =
	sizeof(CombusFrameSof) + sizeof(CombusFrameHeader) +
	kCombusHandshakePayloadLen + sizeof(uint8_t);


/**
 * @brief Bit of the header `flags` byte that marks a handshake as an ACK.
 *
 * @details A handshake without it is a request.  A bidirectional end answers
 *   every request with an ACK carrying its own MD5; nobody answers an ACK.
 */

static constexpr uint8_t kCombusHandshakeFlagAck = 0x01u;


/**
 * @brief Bit of the header `flags` byte that marks a NEXT handshake.
 *
 * @details A NEXT request or ACK carries the MD5 of the next set (stable
 *   channels then candidates); a legacy one carries the stable MD5.  An ACK
 *   has the kind of the request it answers.
 */

static constexpr uint8_t kCombusHandshakeFlagNext = 0x02u;



// =============================================================================
// 2. TIMING
// =============================================================================

/**
 * @brief Number of handshake frames emitted in the boot burst.
 *
 * @details The peer can validate the contract even if the very first frame is
 *   lost on a noisy link.  Each link starts its own burst counter at this value.
 */

static constexpr uint8_t kCombusHandshakeBurstCount = 3u;


	/// Interval between two frames of the boot burst, in ms.
static constexpr uint32_t kCombusHandshakeBurstPeriodMs = 100u;


/**
 * @brief NEXT requests sent after the burst before switching to legacy.
 *
 * @details While the contract is UNKNOWN, an end with candidates sends its
 *   burst and this many periodic requests as NEXT, then legacy requests.  A
 *   NEXT ACK that does not match switches to legacy at once.
 */

static constexpr uint8_t kCombusHandshakeNextRetries = 3u;


/**
 * @brief Period of the handshake requests after the boot burst, in ms.
 *
 * @details Every transmitting end sends one request per period, whatever the
 *   state of the contract: while UNKNOWN to establish it, while STABLE as the
 *   periodic ACK request, and always on a transmit-only end.  Four requests
 *   fit in kCombusHandshakeAckTimeoutMs, so one lost ACK is not a loss.
 */

static constexpr uint32_t kCombusHandshakePeriodMs = 250u;


/**
 * @brief Time without ACK after which a bidirectional end drops its contract, in ms.
 *
 * @details The ACK proves that the peer receives this end; data frames only
 *   prove the reverse.  No ACK for this long: the link is lost.  1 s: a machine
 *   out of control must not run longer.
 */

static constexpr uint32_t kCombusHandshakeAckTimeoutMs = 1000u;



// =============================================================================
// 3. COMPILE-TIME SWITCHES
// =============================================================================

/**
 * @brief Bring-up switch: when non-zero, the MD5 and version compare of the RX
 *   path is short-circuited.  Logged loudly at boot.  Default off.
 */

#ifndef COMBUS_MD5_CHECK_DISABLE
	#define COMBUS_MD5_CHECK_DISABLE  0
#endif


/**
 * @brief Debug switch: when non-zero, the TX side never emits the boot burst;
 *   the contract waits for the first periodic request.  Default off.
 */

#ifndef COMBUS_HANDSHAKE_BURST_DISABLE
	#define COMBUS_HANDSHAKE_BURST_DISABLE  0
#endif



// =============================================================================
// 4. SHARED HELPERS
// =============================================================================

/**
 * @brief Log the boot banner of a link once: MD5 compare state, local MD5, version.
 *
 * @details Idempotent, guarded by `ctx->bootWarningLogged`.  Nothing is logged
 *   on a link not initialised through combus_protocol_init().
 *
 * @param ctx  Per-link handshake context.
 */

void combus_handshake_logBootWarningIfNeeded(CombusHandshakeContext* ctx);


/**
 * @brief Format a 16-byte MD5 as a 32-char lowercase hex string.
 *
 * @details Always NUL-terminates.  The single representation of an MD5 in the logs.
 *
 * @param[in]  md5  The MD5 (16 bytes).
 * @param[out] out  Caller-owned buffer of at least 33 bytes.
 */

void combus_handshake_formatMd5Hex(const uint8_t md5[16], char out[33]);



// =============================================================================
// 5. PER-LINK CONTEXT
// =============================================================================

/**
 * @brief Handshake state of one ComBus link.
 *
 * @details Owned by the caller, must outlive the link.  TX and RX of a link
 *   share the same context; a context is never shared across links.  Two ends
 *   per link: the fields that describe the peer (contract, ACK, mode) become one
 *   set per peer when a link serves several.
 */

struct CombusHandshakeContext {
		// Identity of the link (set by combus_protocol_init())
	const char*        name          = nullptr;  ///< link name, for the logs
	bool               bidirectional = false;    ///< the end both transmits and receives
	const ComPeerLink* peer          = nullptr;  ///< link between nodes: common frame and table; nullptr = view of the scope
	bool               baseSet       = false;    ///< BASE link: the mode sets the size of the frames

		// MD5 of the contract (set by combus_protocol_init())
	ComLinkMd5 expected      = {};       ///< MD5 expected from the peer, compared on every handshake
	ComLinkMd5 announced     = {};       ///< MD5 this end sends when it differs (link through a coupler: its own side); null = `expected`
	bool       hasCandidates = false;    ///< expected.next differs from expected.stable

		// State of the contract
	bool contractValidated = false;      ///< contract STABLE (true) or UNKNOWN (false)
	bool everReceived      = false;      ///< a valid handshake frame was observed, matching or not (witness of the tests)
	bool dataSinceStable   = false;      ///< a data frame was applied since the contract became STABLE
	bool refusalLogged     = false;      ///< one-shot guard for the refusal log, re-armed when STABLE
	bool bootWarningLogged = false;      ///< one-shot guard for the boot banner

		// Requests and ACK
	bool     ackPending     = false;     ///< a request was received, the TX answers it with an ACK
	bool     ackPendingNext = false;     ///< a NEXT request was received, the TX answers it with a NEXT ACK
	uint32_t lastRequestMs  = 0u;        ///< millis() at the last request sent after the burst
	uint32_t lastAckMs      = 0u;        ///< millis() at the last matching ACK (bidirectional end)

		// Boot burst
	bool     burstActive    = false;     ///< the boot burst is in progress
	uint8_t  burstRemaining = 0u;        ///< frames left to emit in the burst
	uint32_t lastBurstMs    = 0u;        ///< millis() at the last burst emission

		// Next / stable mode
	bool    nextMode      = false;       ///< STABLE in next mode (false: stable mode, or legacy without candidate)
	uint8_t nextTriesLeft = 0u;          ///< NEXT requests left while UNKNOWN before legacy ones
};



// =============================================================================
// 6. STATUS
// =============================================================================

/**
 * @brief True if the contract of the link is STABLE, false if it is UNKNOWN.
 *
 * @details STABLE once a handshake with the same MD5 and major version is
 *   received: the ACK of its own request on a bidirectional end, the request of
 *   the peer on a receive-only end.  Back to UNKNOWN on a refused handshake (MD5
 *   and major compared on every one), a silence longer than the link timeout,
 *   missed ACKs (bidirectional end), or a transport (re)initialisation.
 *
 *   While UNKNOWN, the data frames received are not applied and the link is not
 *   alive; a bidirectional end sends no data.
 *
 * @param ctx  Per-link handshake context.
 *
 * @return true if STABLE; false if UNKNOWN or `ctx` is null.
 */

bool combus_handshake_is_contract_validated(const CombusHandshakeContext* ctx);



// =============================================================================
// 7. INTERNAL BRIDGE
// =============================================================================

/**
 * @brief Changes of the per-link context, shared by the handshake RX and TX and
 *   by the transport init.  Not part of the public API.
 *
 * @details Every function is a no-op on a null context.
 */

namespace combus_handshake_internal {

	/**
	 * @brief Record that a valid handshake frame was observed.
	 *
	 * @details The firmware never reads it: the tests do, to prove a frame reached the
	 *   handshake decoder or stayed away from it.
	 *
	 * @param ctx  Per-link handshake context.
	 */

	void markEverReceived(CombusHandshakeContext* ctx);


	/**
	 * @brief Mark the contract of the link as STABLE, on an accepted handshake.
	 *
	 * @param ctx  Per-link handshake context.
	 */

	void markContractValidated(CombusHandshakeContext* ctx);


	/**
	 * @brief Start a fresh RX session: contract UNKNOWN, no pending ACK.
	 *
	 * @details Called by combus_rx_init().  Back to stable mode, NEXT requests
	 *   re-armed when the link has candidates.
	 *
	 * @param ctx  Per-link handshake context.
	 */

	void clearContractValidated(CombusHandshakeContext* ctx);


	/**
	 * @brief Drop a STABLE contract back to UNKNOWN and log why.
	 *
	 * @details Called on a refused handshake, a silence longer than the link
	 *   timeout, or missed ACKs.  No-op when the contract is already UNKNOWN.
	 *
	 * @param ctx     Per-link handshake context.
	 * @param reason  Short text for the log line.
	 */

	void dropContract(CombusHandshakeContext* ctx, const char* reason);


	/**
	 * @brief Arm the boot burst: the first frame leaves on the next TX update.
	 *
	 * @details Called by combus_tx_init().  Calling it twice equals calling it once.
	 *
	 * @param ctx  Per-link handshake context.
	 */

	void startBurst(CombusHandshakeContext* ctx);


	/**
	 * @brief Stop the boot burst, once the contract is STABLE or on a transport reinit.
	 *
	 * @param ctx  Per-link handshake context.
	 */

	void stopBurst(CombusHandshakeContext* ctx);
}

// EOF combus_handshake.h
