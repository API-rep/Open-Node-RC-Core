/******************************************************************************
 * @file  combus_tx.cpp
 * @brief ComBus transmitter: the bus to data frames and handshakes, for every link.
 *
 * @details This file decides what leaves and when.  The formats belong to the
 *   encoders: combus_frame, combus_handshake_tx.
 *****************************************************************************/

#include "combus_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>
#include <core/system/combus/protocol/handshake/combus_handshake_tx.h>
#include <core/system/combus/protocol/combus_rx.h>   // combus_rx_peer_or() — both_or without echo
#include <core/system/combus/combus_access.h>        // combus_both_or_local()
#include <core/system/debug/logging/debug.h>

#include "combus.h"             // AnalogComBusID, DigitalComBusID
#include "combus_system_ids.h"  // size of the SYSTEM view
#include "combus_base_ids.h"    // size of the BASE view



// =============================================================================
// 1. POOL REGISTRY
// =============================================================================

	// The pool of per-link states, owned by the caller.
static CombusTxState* s_txStates = nullptr;
static uint8_t        s_capacity = 0u;


/**
 * @brief Pool registration. Links the module to the storage of the caller, once.
 *
 * @details In order:
 *   1. A second call is refused: the running links keep their storage.
 *   2. An empty pool is refused.
 *   3. The pool is kept for the whole run.
 */

void combus_tx_register_pool( CombusTxState* buffer, uint8_t capacity )
{
		// 1. Second call
	if (s_txStates != nullptr) {
		sys_log_err("[COMBUS_TX] pool already registered — ignoring second call\n");
		return;
	}

		// 2. Empty pool
	if (buffer == nullptr || capacity == 0u) {
		sys_log_err("[COMBUS_TX] register_pool called with null buffer or zero capacity\n");
		return;
	}

		// 3. Keep the pool
	s_txStates = buffer;
	s_capacity = capacity;
}



// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * @brief Transmission start. Prepares the slot of one link and arms its handshake.
 *
 * @details In order:
 *   1. Guards: pool, slot, transport and rate.
 *   2. Transport and layout.  A link starts active; combus_links_watch() suspends a
 *      detached one.
 *   3. Period, by integer division.
 *   4. Channels carried: the size of the view of the layer.  The views are prefixes of
 *      each other, so a count is enough, with no table to build.  Any other layer
 *      halts: a silent fallback would hide a forgotten layer.
 *   5. A link between nodes carries the frame of its table instead.  The context was
 *      attached by combus_link_start() before this call.
 *   6. Handshake burst, as at boot.
 *   7. Log.
 */

void combus_tx_init( uint8_t        linkIdx,
                     NodeCom*       nodeCom,
                     ComBusFrameCfg frameCfg,
                     uint32_t       txHz,
                     ChanLayer      layer )
{
		// 1. Guards
	if (!s_txStates || linkIdx >= s_capacity) { return; }
	if (!nodeCom || txHz == 0u) { return; }

	CombusTxState& st = s_txStates[linkIdx];

		// 2. Transport and layout
	st.nodeCom  = nodeCom;
	st.frameCfg = frameCfg;
	st.active   = true;

		// 3. Period
	st.periodMs = 1000u / txHz;

		// 4. Channels carried by the view of the layer
	switch (layer) {
		case ChanLayer::BASE:
			st.analogWireEnd  = static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT);
			st.digitalWireEnd = static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT);
			break;
		case ChanLayer::SYSTEM:
			st.analogWireEnd  = static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT);
			st.digitalWireEnd = static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT);
			break;
		default:
			sys_log_err("[COMBUS_TX] FATAL: only BASE and SYSTEM travel on a link — system halted\n");
			while (1) { /* halt */ }
	}

		// 5. Link between nodes: the frame of its table
	if (st.handshakeCtx && st.handshakeCtx->peer) {
		st.analogWireEnd  = st.handshakeCtx->peer->analog.count;
		st.digitalWireEnd = st.handshakeCtx->peer->digital.count;
	}

		// 6. Handshake burst
	if (st.handshakeCtx) {
		combus_handshake_internal::startBurst(st.handshakeCtx);
	}

		// 7. Log
	sys_log_info("[COMBUS_TX] init — link=%u  transport='%s'  rate=%uHz  A%u+D%u  wireEnd=A%u+D%u  layer=%u\n",
	             (unsigned)linkIdx,
	             nodeCom->name, (unsigned)txHz,
	             (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital,
	             (unsigned)st.analogWireEnd, (unsigned)st.digitalWireEnd,
	             (unsigned)layer);
}



// =============================================================================
// 3. TRANSMIT UPDATE
// =============================================================================

/**
 * @brief both_or without echo. Prepares the digital values to send on one link.
 *
 * @details A both_or channel carries the own value of this board OR what the OTHER links
 *   received, never what this link sent: otherwise two boards would keep each other's
 *   `true` alive (anti ping-pong, see both_or.md).  The other channels are sent as they
 *   are.  On a link between nodes, positions go through the table of the link.
 *
 *   The values live in a static buffer, consumed by the frame sent just after.
 *
 * @param st       State of the link.
 * @param linkIdx  Slot of this link in the pool.
 * @param bus      The ComBus to send.
 *
 * @return The values to send; nullptr if no channel is both_or: the encoder reads the bus.
 */

static const bool* prepareDigital( const CombusTxState& st, uint8_t linkIdx, const ComBus* bus )
{
	static bool txDigital[255u];

	if (!bus->digitalBus) { return nullptr; }

	const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
	bool anyBothOr = false;

		// Each position of the frame
	for (uint8_t ch = 0u; ch < st.digitalWireEnd && ch < st.frameCfg.nDigital; ++ch) {
		const uint8_t id = peer ? peer->digital.ids[ch] : ch;
		const DigitalComBus& slot = bus->digitalBus[id];

		txDigital[ch] = slot.bothOr.enabled ? (combus_both_or_local(slot) || combus_rx_peer_or(id, linkIdx))
		                                    : slot.value;
		anyBothOr = anyBothOr || slot.bothOr.enabled;
	}

	return anyBothOr ? txDigital : nullptr;
}


/**
 * @brief Trace of a frame sent, with the run level on a board that holds it.
 *
 * @param linkIdx   Slot of this link in the pool.
 * @param seq       Sequence counter of the frame sent.
 * @param frameLen  Length of the frame.
 * @param bus       The ComBus sent.
 */

static void logFrameSent( uint8_t linkIdx, uint8_t seq, uint8_t frameLen, const ComBus* bus )
{
#if defined(HAS_RUNLEVEL_MANAGER)
	output_log_dbg("[COMBUS_TX] link=%u  seq=%u  len=%u  rl=%d\n",
	               (unsigned)linkIdx,
	               (unsigned)seq,
	               (unsigned)frameLen,
	               (int)bus->analogBus[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)].value);
#else
	(void)bus;
	output_log_dbg("[COMBUS_TX] link=%u  seq=%u  len=%u\n",
	               (unsigned)linkIdx, (unsigned)seq, (unsigned)frameLen);
#endif
}


/**
 * @brief Transmission pass. Decides what leaves on each link in turn.
 *
 * @details For each link, in order:
 *   1. Guard: a slot never started, or suspended, sends nothing.
 *   2. Handshake: pending ACK, boot burst, periodic request.  Independent of the timer
 *      of the data frames: both may write in the same pass.
 *   3. A bidirectional end sends no data while its contract is UNKNOWN.  A transmit-only
 *      end always sends: its receivers wait for the contract on their side.
 *   4. Timer of the data frames.
 *   5. Digital values, with both_or without echo.
 *   6. Counts: a BASE link out of next mode sends the stable channels only.  The
 *      candidates follow them on the wire.
 *   7. Encode and send.
 *   8. Next `seq`: wraps from 255 to 1, since 0 is reserved.
 *
 *   The frame lives in a static buffer shared between links: it is written then sent
 *   within the same iteration.
 */

void combus_tx_update( const ComBus* bus )
{
	if (!s_txStates || !bus) { return; }

	static uint8_t frame[255u];

		// Each registered link
	for (uint8_t i = 0u; i < s_capacity; ++i) {
		CombusTxState& st = s_txStates[i];

			// 1. Slot never started, or suspended
		if (!st.nodeCom || st.periodMs == 0u || !st.active) { continue; }

			// 2. Handshake
		if (st.handshakeCtx) {
			combus_handshake_tx_update(st.handshakeCtx, st.nodeCom);
		}

			// 3. No data while the contract of a bidirectional end is UNKNOWN
		if (st.handshakeCtx && st.handshakeCtx->bidirectional && !st.handshakeCtx->contractValidated) {
			continue;
		}

			// 4. Timer of the data frames
		uint32_t now = millis();

		if ((now - st.lastTxMs) < st.periodMs) { continue; }

		st.lastTxMs = now;

			// 5. Digital values
		const bool* digitalValues = prepareDigital(st, i, bus);

			// 6. Counts of this frame
		const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
		uint8_t analogEnd  = st.analogWireEnd;
		uint8_t digitalEnd = st.digitalWireEnd;

		if (st.handshakeCtx && st.handshakeCtx->baseSet && !st.handshakeCtx->nextMode) {
			analogEnd  = peer ? peer->analog.stableCount  : combus::wire::kCombusBaseStableAnalogCount;
			digitalEnd = peer ? peer->digital.stableCount : combus::wire::kCombusBaseStableDigitalCount;
		}

			// 7. Encode and send
		uint8_t frameLen = combus_frame_encode(
			st.frameCfg,
			frame,
			bus,
			st.seq,
			analogEnd,
			digitalEnd,
			digitalValues,
			peer ? peer->analog.ids  : nullptr,
			peer ? peer->digital.ids : nullptr
		);

		if (frameLen == 0u) { continue; }

		st.nodeCom->write(st.nodeCom->ctx, frame, frameLen);
		logFrameSent(i, st.seq, frameLen, bus);

			// 8. Next seq
		st.seq++;

		if (st.seq == 0u) { st.seq = 1u; }
	}
}



// =============================================================================
// 4. CONTROL OF A LINK
// =============================================================================

	/// Set before combus_tx_init(), which arms the burst of this context.
void combus_tx_set_handshake_ctx( uint8_t linkIdx, CombusHandshakeContext* ctx )
{
	if (!s_txStates || linkIdx >= s_capacity) { return; }

	s_txStates[linkIdx].handshakeCtx = ctx;
}


	/// Activated again after a suspension: a new handshake, as at boot.
void combus_tx_set_active( uint8_t linkIdx, bool active )
{
	if (!s_txStates || linkIdx >= s_capacity) { return; }

	CombusTxState& st = s_txStates[linkIdx];

		// Check for a suspended link coming back
	if (active && !st.active) { combus_handshake_internal::startBurst(st.handshakeCtx); }

	st.active = active;
}

// EOF combus_tx.cpp
