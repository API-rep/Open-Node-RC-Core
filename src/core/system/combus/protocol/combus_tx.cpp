/******************************************************************************
 * @file combus_tx.cpp
 * @brief ComBus transmitter — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>
#include <core/system/combus/protocol/frame/combus_handshake_tx.h>
#include <core/system/debug/logging/debug.h>

// LY2 — include the 3 view-specific ID headers so combus_tx_init()
// can resolve analogWireEnd / digitalWireEnd from the link's
// ChanLayer.  combus.h transitively includes combus_ids.h (FULL view).
#include "combus.h"
#include "combus_local_ids.h"
#include "combus_remote_ids.h"



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
// 2. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

void combus_tx_set_handshake_ctx( uint8_t                linkIdx,
                                  CombusHandshakeContext* ctx )
{
    if (!s_txStates || linkIdx >= s_capacity) { return; }
    s_txStates[linkIdx].handshakeCtx = ctx;
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
 *      ChanLayer (REMOTE → CH_COUNT of the Remote view, LOCAL → CH_COUNT
 *      of the Local view, FULL → CH_COUNT of the Full view).  The codec
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

		// --- 3. Derive transmit period ---
	st.periodMs = 1000u / txHz;

		// --- 4. LY2 — resolve wire-end counters from the link's ChanLayer.
	//    The 3 view-specific ID enums share the same backing storage
	//    (see Group C in test_combus_loopback.cpp) and form a contiguous
	//    prefix — so the wire-end counter is simply the CH_COUNT of the
	//    matching view.  No table to build, just two counters.
	//
	//    Only REMOTE, LOCAL and SYSTEM are valid for a TX/RX link.
	//    UNDEFINED is a config error (forgotten / missing layer) and
	//    triggers a FATAL halt — a silent fallback to FULL would mask
	//    the bug instead of revealing it (same pattern as the
	//    "no pool registered" FATAL in uart_com.cpp).
	switch (layer) {
		case ChanLayer::REMOTE:
			st.analogWireEnd  = static_cast<uint8_t>(AnalogComBusRemoteID::CH_COUNT);
			st.digitalWireEnd = static_cast<uint8_t>(DigitalComBusRemoteID::CH_COUNT);
			break;
		case ChanLayer::LOCAL:
			st.analogWireEnd  = static_cast<uint8_t>(AnalogComBusLocalID::CH_COUNT);
			st.digitalWireEnd = static_cast<uint8_t>(DigitalComBusLocalID::CH_COUNT);
			break;
		case ChanLayer::SYSTEM:
			// SYSTEM — reserved for a future intra-device loopback.
			// Falls back to the FULL view (machine node default).
			st.analogWireEnd  = static_cast<uint8_t>(AnalogComBusID::CH_COUNT);
			st.digitalWireEnd = static_cast<uint8_t>(DigitalComBusID::CH_COUNT);
			break;
		case ChanLayer::UNDEFINED:
		default:
			// UNDEFINED — config error (forgotten / missing layer).
			// FATAL halt: a silent fallback to FULL would mask the
			// bug instead of revealing it.  Same pattern as the
			// "no pool registered" FATAL in uart_com.cpp.
			sys_log_err("[COMBUS_TX] FATAL: ChanLayer::UNDEFINED is not a valid layer for a TX/RX link — system halted\n");
			while (1) { /* halt */ }
			break;
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
	             nodeCom->name, txHz,
	             (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital,
	             (unsigned)st.analogWireEnd, (unsigned)st.digitalWireEnd,
	             (unsigned)layer);
}




// =============================================================================
// 4. TRANSMIT UPDATE
// =============================================================================

/**
 * @brief Encode and transmit one ComBus frame if the period has elapsed.
 *
 * @details Timer-gated, non-blocking — safe to call every loop.  Iterates
 *   over every link registered by combus_tx_register_pool(); each link
 *   has its own timer gate, sequence counter, and handshake burst state.
 *   1. Guard check — returns immediately if uninit or bus pointer is null.
 *   2. Timer gate — returns if the transmit period has not elapsed.
 *   3. Encode the ComBus state into a binary frame via `combus_frame_encode()`.
 *      LY2 — the codec iterates only over comBus.analogBus[0..analogWireEnd)
 *      and comBus.digitalBus[0..digitalWireEnd) — the wire payload is
 *      sized to the layer's prefix length.
 *   4. Send the frame through the transport interface and advance the sequence counter.
 */

void combus_tx_update(
    const ComBus* bus)      // live ComBus state to encode
{
		// --- 0. Top-level guard ---
	if (!s_txStates || !bus) { return; }

		// Shared scratch buffer — written then consumed synchronously inside
		// the loop body, so no overlap between iterations is possible.
	static uint8_t frame[255u];

		// --- Iterate over every registered link ---
	for (uint8_t i = 0u; i < s_capacity; ++i) {
		CombusTxState& st = s_txStates[i];

			// --- 1. Per-link guard check ---
		if (!st.nodeCom || st.periodMs == 0u) { continue; }

			// --- 1b. P3 — drive the boot-time handshake burst on the linked
		//    context.  Independent of the control-frame timer below — if
		//    both timers expire on the same call, two write() calls happen
		//    back-to-back (no priority / contention logic, per P3 constraint
		//    #2).  No-op when the burst is inactive or already validated.
		if (st.handshakeCtx) {
			combus_handshake_tx_update(st.handshakeCtx, st.nodeCom);
		}

			// --- 2. Timer gate ---
		uint32_t now = millis();
		if ((now - st.lastTxMs) < st.periodMs) { continue; }
		st.lastTxMs = now;


			// --- 3. Encode ---
			// LY2 — pass st.analogWireEnd / st.digitalWireEnd (resolved
			// at TX init from link->layer) so the codec iterates only
			// over the layer's prefix length.  This is the ONLY
			// behavioural change vs. the pre-LY2 codec.
		uint8_t frameLen = combus_frame_encode(
		    st.frameCfg,
		    frame,
		    bus,
		    st.seq,
		    st.analogWireEnd,
		    st.digitalWireEnd
		);

		if (frameLen == 0u) { continue; }

			// --- 4. Send via transport ---
		st.nodeCom->write(st.nodeCom->ctx, frame, frameLen);

			// Capture the seq value that was actually written on the wire BEFORE
		//    advancing — used by the debug log below. Important because the new
		//    wrap rule (255 → 1, never 0) breaks the old "st.seq - 1u"
		//    trick: after a real wrap, seq==1 and that expression would print
		//    0, which is misleading since value 0 is RESERVED for handshake
		//    frames (see combus_handshake.h) and must NEVER appear in a control
		//    frame log line.
		const uint8_t seqSent = st.seq;

			// Advance seq counter. The control-frame range is 1..255 — value 0 is
		//    reserved for future handshake frames (see combus_handshake.h) and must
		//    never appear in a normal frame on the wire. Wrap from 255 → 1.
		st.seq++;
		if (st.seq == 0u) { st.seq = 1u; }

		// RL3: runLevel is now a plain analog channel — read from analogBus[RUNLEVEL].
		output_log_dbg("[COMBUS_TX] link=%u  seq=%u  len=%u  rl=%d\n",
		               (unsigned)i,
		               (unsigned)seqSent,
		               (unsigned)frameLen,
		               (int)bus->analogBus[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)].value);
	}
}



// EOF combus_tx.cpp