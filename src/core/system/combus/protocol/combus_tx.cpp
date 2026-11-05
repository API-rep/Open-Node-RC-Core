/******************************************************************************
 * @file combus_tx.cpp
 * @brief ComBus transmitter — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>
#include <core/system/combus/protocol/handshake/combus_handshake_tx.h>
#include <core/system/combus/protocol/combus_rx.h>   // combus_rx_peer_or() — both_or without echo
#include <core/system/combus/combus_access.h>        // combus_both_or_local()
#include <core/system/debug/logging/debug.h>

// LY2 — include the 3 view-specific ID headers so combus_tx_init()
// can resolve analogWireEnd / digitalWireEnd from the link's
// ChanLayer.  combus.h transitively includes combus_ids.h (FULL view).
#include "combus.h"
#include "combus_system_ids.h"
#include "combus_base_ids.h"



// =============================================================================
// 1. POOL REGISTRY  (externally-owned storage — see combus_tx_register_pool)
// =============================================================================

// Per-link state array — caller-owned static storage, registered at boot
// via combus_tx_register_pool().  Core never allocates this.  The number
// of links is decided by the caller (typically the size of the
// ComBusLink[] array passed to combus_protocol_init_all()); this module
// never imposes a hard cap.
static CombusTxState* s_txStates = nullptr;
static uint8_t        s_capacity = 0u;

void combus_tx_register_pool( CombusTxState* buffer, uint8_t capacity )
{
	if (s_txStates != nullptr) {
		sys_log_err("[COMBUS_TX] pool already registered — ignoring second call\n");
		return;
	}
	if (buffer == nullptr || capacity == 0u) {
		sys_log_err("[COMBUS_TX] register_pool called with null buffer or zero capacity\n");
		return;
	}
	s_txStates = buffer;
	s_capacity = capacity;
}



// =============================================================================
// 3. INITIALIZATION
// =============================================================================

/**
 * @brief Initialize the ComBus transmitter for one link.
 *
 * @details Initialization sequence:
 *   1. Guard check — null transport pointer, zero rate, or out-of-range
 *      linkIdx; returns immediately.
 *   2. Store the transport interface and frame layout descriptor.
 *   3. Derive the transmit period from txHz (integer division, ms).
 *   4. LY2 — resolve analogWireEnd / digitalWireEnd from the link's
 *      ChanLayer (BASE → CH_COUNT of the Base view, SYSTEM → CH_COUNT
 *      of the System view, FULL → CH_COUNT of the Full view).  The codec
 *      iterates only over comBus.analogBus[0..analogWireEnd) and
 *      comBus.digitalBus[0..digitalWireEnd) — this is the ONLY
 *      behavioural change vs. the pre-LY2 codec.
 *   5. Reset handshake burst state on the linked context (P3).
 *   6. Log init confirmation.
 */

void combus_tx_init(
	uint8_t        linkIdx,  // index of this link in the per-link state array
	NodeCom*       nodeCom,  // claimed transport interface (from uart_com_init or similar)
	ComBusFrameCfg frameCfg, // static frame layout descriptor (nAnalog, nDigital)
	uint32_t       txHz,     // frame transmit rate in Hz
	ChanLayer      layer )   // LY2 — selects the wire-end counters

{
		// --- 1. Guard check ---
	if (!s_txStates || linkIdx >= s_capacity) { return; }
	if (!nodeCom || txHz == 0u) { return; }

	CombusTxState& st = s_txStates[linkIdx];

		// --- 2. Store transport interface and frame layout ---
	st.nodeCom  = nodeCom;
	st.frameCfg = frameCfg;
	st.active   = true;     // 3.3: a link starts active; combus_links_watch() suspends an unhitched one

		// --- 3. Derive transmit period ---
	st.periodMs = 1000u / txHz;

		// --- 4. LY2 — resolve wire-end counters from the link's ChanLayer.
	//    The 3 view-specific ID enums share the same backing storage
	//    (see Group C in test_combus_loopback.cpp) and form a contiguous
	//    prefix — so the wire-end counter is simply the CH_COUNT of the
	//    matching view.  No table to build, just two counters.
	//
	//    Only BASE, SYSTEM and BOARD are valid for a TX/RX link.
	//    UNDEFINED is a config error (forgotten / missing layer) and
	//    triggers a FATAL halt — a silent fallback to FULL would mask
	//    the bug instead of revealing it (same pattern as the
	//    "no pool registered" FATAL in uart_com.cpp).
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

		// --- 4b. Link rework 3.1b — a link between nodes carries the common frame
		//    of the link (wired by combus_protocol_init() before this call).
	if (st.handshakeCtx && st.handshakeCtx->peer) {
		st.analogWireEnd  = st.handshakeCtx->peer->analog.count;
		st.digitalWireEnd = st.handshakeCtx->peer->digital.count;
	}

		// --- 5. Reset handshake burst state on the linked context (P3).
	//    combus_protocol_init() wires st.handshakeCtx to the same
	//    context as the RX side BEFORE calling combus_tx_init().
	//    If the wiring is missing (legacy caller), the burst is simply
	//    not armed — no crash, no UB.
	if (st.handshakeCtx) {
		combus_handshake_internal::startBurst(st.handshakeCtx);
	}

		// --- 6. Log init confirmation ---
	sys_log_info("[COMBUS_TX] init — link=%u  transport='%s'  rate=%uHz  A%u+D%u  wireEnd=A%u+D%u  layer=%u\n",
	             (unsigned)linkIdx,
	             nodeCom->name, (unsigned)txHz,
	             (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital,
	             (unsigned)st.analogWireEnd, (unsigned)st.digitalWireEnd,
	             (unsigned)layer);
}




// =============================================================================
// 4. TRANSMIT UPDATE
// =============================================================================

static const bool* prepareDigital( const CombusTxState& st, uint8_t linkIdx, const ComBus* bus )
{
	static bool txDigital[255u];

	if (!bus->digitalBus) { return nullptr; }

	const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
	bool anyBothOr = false;

	for (uint8_t ch = 0u; ch < st.digitalWireEnd && ch < st.frameCfg.nDigital; ++ch) {
		const uint8_t id = peer ? peer->digital.ids[ch] : ch;
		const DigitalComBus& slot = bus->digitalBus[id];

		txDigital[ch] = slot.bothOr.enabled ? (combus_both_or_local(slot) || combus_rx_peer_or(id, linkIdx))
		                                    : slot.value;
		anyBothOr = anyBothOr || slot.bothOr.enabled;
	}

	return anyBothOr ? txDigital : nullptr;
}


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


void combus_tx_update( const ComBus* bus )
{
	if (!s_txStates || !bus) { return; }

	static uint8_t frame[255u];

	for (uint8_t i = 0u; i < s_capacity; ++i) {
		CombusTxState& st = s_txStates[i];

		if (!st.nodeCom || st.periodMs == 0u || !st.active) { continue; }

		if (st.handshakeCtx) {
			combus_handshake_tx_update(st.handshakeCtx, st.nodeCom);
		}

		if (st.handshakeCtx && st.handshakeCtx->bidirectional && !st.handshakeCtx->contractValidated) {
			continue;
		}

		uint32_t now = millis();

		if ((now - st.lastTxMs) < st.periodMs) { continue; }

		st.lastTxMs = now;

		const bool* digitalValues = prepareDigital(st, i, bus);

		const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
		uint8_t analogEnd  = st.analogWireEnd;
		uint8_t digitalEnd = st.digitalWireEnd;

		if (st.handshakeCtx && st.handshakeCtx->baseSet && !st.handshakeCtx->nextMode) {
			analogEnd  = peer ? peer->analog.stableCount  : combus::wire::kCombusBaseStableAnalogCount;
			digitalEnd = peer ? peer->digital.stableCount : combus::wire::kCombusBaseStableDigitalCount;
		}

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

		st.seq++;

		if (st.seq == 0u) { st.seq = 1u; }
	}
}



// =============================================================================
// 2. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

void combus_tx_set_handshake_ctx( uint8_t                linkIdx,
                                  CombusHandshakeContext* ctx )
{
	if (!s_txStates || linkIdx >= s_capacity) { return; }
	s_txStates[linkIdx].handshakeCtx = ctx;
}


void combus_tx_set_active( uint8_t linkIdx, bool active )
{
	if (!s_txStates || linkIdx >= s_capacity) { return; }
	CombusTxState& st = s_txStates[linkIdx];
	if (active && !st.active) { combus_handshake_internal::startBurst(st.handshakeCtx); }   // a new handshake, as at boot
	st.active = active;
}

// EOF combus_tx.cpp