/******************************************************************************
 * @file  combus_rx.cpp
 * @brief ComBus receiver: bytes of a transport to decoded frames, for every link.
 *
 * @details This file routes the bytes and follows the state of each link.  The
 *   formats belong to the decoders: combus_frame, combus_handshake_rx, combus_coupling.
 *****************************************************************************/

#include "combus_rx.h"

#include <stddef.h>
#include <Arduino.h>

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>
#include <core/system/combus/protocol/handshake/combus_handshake_rx.h>  // combus_handshake_tryDecode
#include <core/system/combus/protocol/coupling/combus_coupling.h>       // combus_coupling_tryDecode
#include <core/system/debug/logging/debug.h>



// =============================================================================
// 1. POOL REGISTRY
// =============================================================================

	// The pool of per-link states, owned by the caller.
static CombusRxState* s_rxStates = nullptr;
static uint8_t        s_capacity = 0u;


/**
 * @brief Pool registration. Links the module to the storage of the caller, once.
 *
 * @details In order:
 *   1. A second call is refused: the running links keep their storage.
 *   2. An empty pool is refused.
 *   3. The pool is kept for the whole run.
 */

void combus_rx_register_pool( CombusRxState* buffer, uint8_t capacity )
{
		// 1. Second call
	if (s_rxStates != nullptr) {
		sys_log_err("[COMBUS_RX] pool already registered — ignoring second call\n");
		return;
	}

		// 2. Empty pool
	if (buffer == nullptr || capacity == 0u) {
		sys_log_err("[COMBUS_RX] register_pool called with null buffer or zero capacity\n");
		return;
	}

		// 3. Keep the pool
	s_rxStates = buffer;
	s_capacity = capacity;
}



// =============================================================================
// 2. RING BUFFER
// =============================================================================

/**
 * @brief Ring writer. Adds one byte; when full, the oldest byte is lost.
 *
 * @param st    State of the link.
 * @param byte  Byte received.
 */

static void rxBufPush( CombusRxState& st, uint8_t byte )
{
		// Room left: write after the last byte
	if (st.rxCount < CombusRxBufSize) {
		uint8_t idx = (uint8_t)((st.rxHead + st.rxCount) % CombusRxBufSize);
		st.rxBuf[idx] = byte;
		st.rxCount++;
	} else {
			// Full: overwrite the oldest byte
		st.rxBuf[st.rxHead] = byte;
		st.rxHead = (uint8_t)((st.rxHead + 1u) % CombusRxBufSize);
	}
}


/**
 * @brief Ring reader. Gives the byte at a logical position, wrapping around the end.
 *
 * @param st  State of the link.
 * @param i   Logical position, 0 = oldest.
 *
 * @return The byte at that position.
 */

static uint8_t rxBufAt( const CombusRxState& st, uint8_t i )
{
	return st.rxBuf[(st.rxHead + i) % CombusRxBufSize];
}


/**
 * @brief Ring consumer. Forgets the oldest bytes.
 *
 * @param st  State of the link.
 * @param n   Number of bytes to forget, clamped to the bytes held.
 */

static void rxBufConsume( CombusRxState& st, uint8_t n )
{
		// Check that the bytes exist
	if (n > st.rxCount) { n = st.rxCount; }

	st.rxHead  = (uint8_t)((st.rxHead + n) % CombusRxBufSize);
	st.rxCount = (uint8_t)(st.rxCount - n);
}



// =============================================================================
// 3. DECODING
// =============================================================================

/**
 * @brief Snapshot to bus bridge. Writes the frame just accepted to the target of the link.
 *
 * @details In order:
 *   1. No target: nothing to write, the frame stays in the snapshot.
 *   2. Counts: a BASE link out of next mode writes the stable channels only, so the
 *      candidates keep their default value.
 *   3. Write, with the layer of the link as identity and its table, if it has one.
 *
 * @param st  State of the link.
 */

static void applyToBus( CombusRxState& st )
{
		// 1. No target
	if (st.target == nullptr) { return; }

		// 2. Counts of this frame
	const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
	ComBusFrameCfg applyCfg = st.frameCfg;

	if (st.handshakeCtx && st.handshakeCtx->baseSet && !st.handshakeCtx->nextMode) {
		applyCfg.nAnalog  = peer ? peer->analog.stableCount  : combus::wire::kCombusBaseStableAnalogCount;
		applyCfg.nDigital = peer ? peer->digital.stableCount : combus::wire::kCombusBaseStableDigitalCount;
	}

		// 3. Write to the bus
	combus_frame_apply(applyCfg, st.target, &st.snap, st.writeLayer, peer);
}


/**
 * @brief Data frame path. Takes the data frame at the head of the ring, if it is whole and valid.
 *
 * @details In order:
 *   1. Header present, then length from the counts it announces.  An impossible
 *      length means a false SOF: one byte is dropped.
 *   2. Wait for the whole frame.
 *   3. Linear copy: the frame may cross the end of the ring.
 *   4. Decode.  A refused frame costs one byte only, and the search goes on from the
 *      next one: a false SOF never swallows the good frame behind it.
 *   5. Contract UNKNOWN: a valid frame is consumed but ignored.  The snapshot is
 *      invalidated and the link stays not alive, so that a peer with another contract
 *      counts as never seen.
 *   6. Frame accepted: the link is alive, the frame goes to the bus.
 *
 * @param st  State of the link.
 *
 * @return Number of bytes of the frame consumed, or 0 if the caller must stop for this
 *   poll: incomplete frame, or a byte dropped.
 */

static uint8_t decodeDataFrame( CombusRxState& st )
{
		// 1. Length announced by the header
	if (st.rxCount < CombusFrameMinLen) {
		return 0u;
	}

	uint8_t nAnalog   = rxBufAt(st, 1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog));
	uint8_t nDigital  = rxBufAt(st, 1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital));
	uint8_t nDigBytes = (nDigital + 7u) / 8u;

	uint16_t expectedLenW = CombusFrameHeaderLen
	                      + (uint16_t)nDigBytes
	                      + (uint16_t)nAnalog * 2u
	                      + 1u;

		// Check for an impossible length: a false SOF
	if (expectedLenW > CombusRxBufSize) {
		rxBufConsume(st, 1u);
		return 0u;
	}

		// 2. Wait for the whole frame
	uint8_t expectedLen = (uint8_t)expectedLenW;

	if (st.rxCount < expectedLen) {
		return 0u;
	}

		// 3. Linear copy
	uint8_t linear[CombusRxBufSize];

	for (uint8_t i = 0u; i < expectedLen; ++i) {
		linear[i] = rxBufAt(st, i);
	}

		// 4. Decode: a refused frame costs one byte
	if (!combus_frame_decode(st.frameCfg, &st.snap, linear, expectedLen)) {
		rxBufConsume(st, 1u);
		return 0u;
	}

		// 5. Contract UNKNOWN: consumed but ignored
	if (st.handshakeCtx && !st.handshakeCtx->contractValidated) {
		st.snapValid = false;
		rxBufConsume(st, expectedLen);
		return expectedLen;
	}

		// 6. Frame accepted
	st.snapValid    = true;
	st.lastRxMs     = millis();
	st.everReceived = true;

		// Set the proof that data flows: the watch of the silence starts here
	if (st.handshakeCtx) { st.handshakeCtx->dataSinceStable = true; }

	applyToBus(st);
	rxBufConsume(st, expectedLen);

	return expectedLen;
}


/**
 * @brief Frame router. Sends the head of the ring to the decoder of its kind of frame.
 *
 * @details In order:
 *   1. SOF scan: the bytes before the next SOF are dropped.
 *   2. Header present, otherwise wait for more bytes.
 *   3. `seq == 0` with the coupling flag: the coupling decoder.
 *   4. While pairing, only coupling frames count: any other byte is dropped.
 *   5. Any other `seq == 0`: the handshake decoder, which owns its CRC and its
 *      consumption.
 *   6. `seq` in 1..255: a data frame.
 *
 * @param linkIdx  Slot of this link in the pool.
 * @param st       State of the link.
 *
 * @return Number of bytes consumed, or 0 if the caller must stop for this poll.
 */

static uint8_t tryDecode( uint8_t linkIdx, CombusRxState& st )
{
		// 1. SOF scan
	while (st.rxCount > 0u && rxBufAt(st, 0u) != CombusFrameSof) {
		rxBufConsume(st, 1u);
	}

		// 2. Header present
	if (st.rxCount < CombusFrameHeaderLen) {
		return 0u;
	}

	uint8_t seqByte = rxBufAt(st, 1u + offsetof(CombusFrameHeader, seq));

		// 3. Coupling frame
	if (seqByte == 0u) {
		const uint8_t flagsByte = rxBufAt(st, 1u + offsetof(CombusFrameHeader, flags));

			// Check for the coupling flag
		if (flagsByte & kCombusFrameFlagCoupling) {
			return combus_coupling_tryDecode(linkIdx, st.rxBuf, CombusRxBufSize, st.rxHead, st.rxCount);
		}
	}

		// 4. Pairing: coupling frames only
	if (st.mode == CombusRxMode::PAIRING) {
		rxBufConsume(st, 1u);
		return 1u;
	}

		// 5. Handshake
	if (seqByte == 0u) {
		return combus_handshake_tryDecode(st.handshakeCtx, st.rxBuf, CombusRxBufSize, st.rxHead, st.rxCount);
	}

		// 6. Data frame
	return decodeDataFrame(st);
}



// =============================================================================
// 4. PER-LINK INIT
// =============================================================================

/**
 * @brief Reception start. Prepares the slot of one link, with no trace of a previous run.
 *
 * @details In order:
 *   1. Guards: pool, slot and pointers.
 *   2. Transport and layout.  A link starts ON; combus_links_watch() turns a coupled
 *      one off.
 *   3. Write identity.  With a target, only BASE and SYSTEM are accepted: a silent
 *      fallback would hide a forgotten layer, and a frame never writes a BOARD channel.
 *   4. Output buffers of the caller.
 *   5. Empty ring, no frame, and a contract that starts UNKNOWN.
 *   6. Log.
 */

void combus_rx_init( uint8_t        linkIdx,
                     NodeCom*       nodeCom,
                     ComBusFrameCfg frameCfg,
                     uint16_t*      analogBuf,
                     bool*          digitalBuf,
                     ComBus*        target,
                     ChanLayer      layer )
{
		// 1. Guards
	if (!s_rxStates || linkIdx >= s_capacity) { return; }
	if (!nodeCom || !analogBuf || !digitalBuf) { return; }

	CombusRxState& st = s_rxStates[linkIdx];

		// 2. Transport and layout
	st.nodeCom  = nodeCom;
	st.frameCfg = frameCfg;
	st.mode     = CombusRxMode::ON;
	st.target   = target;

		// 3. Write identity
	st.writeLayer = layer;

	if (target != nullptr && layer != ChanLayer::BASE && layer != ChanLayer::SYSTEM) {
		sys_log_err("[COMBUS_RX] FATAL: only BASE and SYSTEM travel on a link — system halted\n");
		while (1) { /* halt */ }
	}

		// 4. Output buffers
	st.snap.analog  = analogBuf;
	st.snap.digital = digitalBuf;

		// 5. Fresh state
	st.rxHead       = 0u;
	st.rxCount      = 0u;
	st.snapValid    = false;
	st.everReceived = false;

	combus_handshake_internal::clearContractValidated(st.handshakeCtx);

		// 6. Log
	sys_log_info("[COMBUS_RX] init — link=%u  transport='%s'  A%u+D%u  layer=%u\n",
	             (unsigned)linkIdx,
	             nodeCom->name,
	             (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital,
	             (unsigned)layer);
}



// =============================================================================
// 5. RECEIVE UPDATE
// =============================================================================

/**
 * @brief Reception pass. Reads, decodes and watches each link in turn.
 *
 * @details For each link, in order:
 *   1. Guard: a slot never started is skipped.
 *   2. A link OFF drains its transport and keeps nothing.
 *   3. All the available bytes go to the ring.
 *   4. Decode until a decoder has nothing left to consume.  Several frames may have
 *      arrived since the last pass.
 *   5. Watch of a STABLE contract.  The silence only counts once data has flowed: a
 *      receiver may validate its contract on a request, before any data frame.  The
 *      missing ACK only concerns a bidirectional end: the peer no longer receives it.
 */

void combus_rx_update( void )
{
	if (!s_rxStates) { return; }

		// Each registered link
	for (uint8_t i = 0u; i < s_capacity; ++i) {
		CombusRxState& st = s_rxStates[i];

			// 1. Slot never started
		if (!st.nodeCom) { continue; }

			// 2. Link OFF: drain and forget
		if (st.mode == CombusRxMode::OFF) {
			while (st.nodeCom->available(st.nodeCom->ctx) > 0) { st.nodeCom->readByte(st.nodeCom->ctx); }
			continue;
		}

			// 3. Bytes to the ring
		while (st.nodeCom->available(st.nodeCom->ctx) > 0) {
			int b = st.nodeCom->readByte(st.nodeCom->ctx);

				// Check that a byte came back
			if (b >= 0) { rxBufPush(st, (uint8_t)b); }
		}

			// 4. Decode what is complete
		while (st.rxCount >= CombusFrameMinLen) {
			if (tryDecode(i, st) == 0u) {
				break;
			}
		}

			// 5. Watch of a STABLE contract
		CombusHandshakeContext* ctx = st.handshakeCtx;

		if (!ctx || !ctx->contractValidated) { continue; }

		const uint32_t now = millis();

			// Check for a silence after data, then for a missing ACK
		if (ctx->dataSinceStable && st.linkTimeoutMs != 0u && (now - st.lastRxMs) > st.linkTimeoutMs) {
			combus_handshake_internal::dropContract(ctx, "silence");
		} else if (ctx->bidirectional && (now - ctx->lastAckMs) > kCombusHandshakeAckTimeoutMs) {
			combus_handshake_internal::dropContract(ctx, "no ACK, the peer does not receive this end");
		}
	}
}



// =============================================================================
// 6. STATE OF A LINK
// =============================================================================

	/// Last frame accepted, only while it is valid: nullptr once the contract has dropped it.
const ComBusFrame* combus_rx_snapshot( uint8_t linkIdx )
{
	if (!s_rxStates || linkIdx >= s_capacity) { return nullptr; }

	const CombusRxState& st = s_rxStates[linkIdx];

	return st.snapValid ? &st.snap : nullptr;
}


	/// Age of the last frame, by unsigned difference: millis() may wrap around.
uint32_t combus_rx_age_ms( uint8_t linkIdx )
{
	if (!s_rxStates || linkIdx >= s_capacity) { return UINT32_MAX; }

	const CombusRxState& st = s_rxStates[linkIdx];

		// Check that a frame exists
	if (!st.everReceived) {
		return UINT32_MAX;
	}

	return (uint32_t)(millis() - st.lastRxMs);
}


	/// A link is alive under a valid contract only: a frame refused does not count.
bool combus_rx_is_alive( uint8_t linkIdx, uint32_t timeoutMs )
{
	if (!s_rxStates || linkIdx >= s_capacity) { return false; }

	const CombusRxState& st = s_rxStates[linkIdx];
	const bool contractOk = !st.handshakeCtx || st.handshakeCtx->contractValidated;

	return contractOk && st.everReceived && (combus_rx_age_ms(linkIdx) < timeoutMs);
}


	/// Set by the watch of the links, from the description of the link.
void combus_rx_set_link_timeout( uint8_t linkIdx, uint32_t timeoutMs )
{
	if (!s_rxStates || linkIdx >= s_capacity) { return; }

	s_rxStates[linkIdx].linkTimeoutMs = timeoutMs;
}


/**
 * @brief Coupling to reception bridge. Follows the state of the coupling of one link.
 *
 * @details Leaving ON, the link forgets its frames and its contract, so that the next
 *   hitch starts as never seen.  The other changes keep the state.
 */

void combus_rx_set_mode( uint8_t linkIdx, CombusRxMode mode )
{
	if (!s_rxStates || linkIdx >= s_capacity) { return; }

	CombusRxState& st = s_rxStates[linkIdx];
	const CombusRxMode before = st.mode;

	st.mode = mode;

		// Check that the link leaves ON
	if (mode == CombusRxMode::ON || before != CombusRxMode::ON) { return; }

		// Forget the link
	st.rxCount      = 0u;
	st.snapValid    = false;
	st.everReceived = false;

	combus_handshake_internal::dropContract(st.handshakeCtx, "uncoupled");
}


/**
 * @brief both_or merge. Says whether a live link, other than the excluded one, received true.
 *
 * @details For each link, in order:
 *   1. Only the live links count, apart from the excluded one.
 *   2. Wire position of the channel: its ID, or through the table of a link between
 *      nodes, where this node must receive it.
 *   3. The last frame must have carried this position.
 */

bool combus_rx_peer_or( uint8_t ch, uint8_t excludeLink )
{
	if (!s_rxStates) { return false; }

		// Each registered link
	for (uint8_t i = 0u; i < s_capacity; ++i) {
		const CombusRxState& st = s_rxStates[i];

			// 1. Live links only
		if (i == excludeLink || !st.nodeCom || !st.snap.digital || st.linkTimeoutMs == 0u) { continue; }
		if (!combus_rx_is_alive(i, st.linkTimeoutMs)) { continue; }

			// 2. Wire position of the channel
		uint8_t pos = ch;
		const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;

		if (peer) {
			pos = peer->digital.count;

				// Find the position this node receives; none leaves `pos` out of range
			for (uint8_t p = 0u; p < peer->digital.count; ++p) {
				if (peer->digital.ids[p] == ch && peer->digital.rx[p]) { pos = p; break; }
			}
		}

			// 3. Carried by the last frame
		if (pos >= st.snap.header.cfg.nDigital) { continue; }

		if (st.snap.digital[pos]) { return true; }
	}

	return false;
}



// =============================================================================
// 7. HANDSHAKE CONTEXT
// =============================================================================

	/// Set before combus_rx_init(), which starts the contract of this context as UNKNOWN.
void combus_rx_set_handshake_ctx( uint8_t linkIdx, CombusHandshakeContext* ctx )
{
	if (!s_rxStates || linkIdx >= s_capacity) { return; }

	s_rxStates[linkIdx].handshakeCtx = ctx;
}

// EOF combus_rx.cpp
