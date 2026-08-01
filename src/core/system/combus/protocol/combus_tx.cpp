/******************************************************************************
 * @file combus_tx.cpp
 * @brief ComBus transmitter � transport-agnostic implementation.
 *****************************************************************************/

#include "combus_tx.h"

#include <core/system/combus/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/**
 * @brief Persistent state of the ComBus transmitter module.
 *
 * @details Filled once by `combus_tx_init()` and read every cycle by
 *   `combus_tx_update()`. Holds the transport interface, the static frame
 *   layout, the rolling sequence counter, and the timer state needed for
 *   the transmit rate gate.
 *
 *   Lifetime: static � valid for the entire program run after init.
 */

struct CombusTxState {
	NodeCom*        nodeCom  = nullptr;  ///< active transport interface
	ComBusFrameCfg  frameCfg       = {};  ///< static layout descriptor (nAnalog, nDigital)

	uint8_t         seq       = 1u;  ///< rolling frame sequence counter (1..255). Value 0 is RESERVED for future handshake frames.
	uint32_t        lastTxMs  = 0u;  ///< timestamp of last transmitted frame (ms)
	uint32_t        periodMs  = 0u;  ///< transmit period derived from txHz (0 = uninit)
};


static CombusTxState comBusTx;  ///< Combus transmitter instance state



// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * @brief Initialize the ComBus transmitter.
 *
 * @details Initialization sequence:
 *   1. Guard check � null transport pointer or zero rate; returns immediately.
 *   2. Store the transport interface and frame layout descriptor.
 *   3. Derive the transmit period from txHz (integer division, ms).
 *   4. Log init confirmation.
 */

void combus_tx_init(
    NodeCom*       nodeCom,  // claimed transport interface (from uart_com_init or similar)
    ComBusFrameCfg frameCfg, // static frame layout descriptor (nAnalog, nDigital)
    uint32_t       txHz )    // frame transmit rate in Hz

{
		// --- 1. Guard check ---
	if (!nodeCom || txHz == 0u) { return; }

		// --- 2. Store transport interface and frame layout ---
	comBusTx.nodeCom = nodeCom;
	comBusTx.frameCfg = frameCfg;

		// --- 3. Derive transmit period ---
	comBusTx.periodMs = 1000u / txHz;

		// --- 4. Log init confirmation ---
	sys_log_info("[COMBUS_TX] init — transport='%s'  rate=%uHz  A%u+D%u\n",
	             nodeCom->name, txHz, (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital);
}



// =============================================================================
// 3. TRANSMIT UPDATE
// =============================================================================

/**
 * @brief Encode and transmit one ComBus frame if the period has elapsed.
 *
 * @details Timer-gated, non-blocking � safe to call every loop:
 *   1. Guard check � returns immediately if uninit or bus pointer is null.
 *   2. Timer gate � returns if the transmit period has not elapsed.
 *   3. Encode the ComBus state into a binary frame via `combus_frame_encode()`.
 *   4. Send the frame through the transport interface and advance the sequence counter.
 */

void combus_tx_update(
    const ComBus* bus,      // live ComBus state to encode
    bool          failSafe) // set true to flag the frame as failsafe-active
{
		// --- 1. Guard check ---
	if (!comBusTx.nodeCom || comBusTx.periodMs == 0u || !bus) { return; }

		// --- 2. Timer gate ---
	uint32_t now = millis();
	if ((now - comBusTx.lastTxMs) < comBusTx.periodMs) { return; }
	comBusTx.lastTxMs = now;

		// --- 3. Encode ---
	static uint8_t frame[255u];
	uint8_t frameLen = combus_frame_encode(
	    comBusTx.frameCfg,
	    frame,
	    bus,
	    comBusTx.seq,
	    failSafe
	);

	if (frameLen == 0u) { return; }

		// --- 4. Send via transport ---
	comBusTx.nodeCom->write(comBusTx.nodeCom->ctx, frame, frameLen);

		// Capture the seq value that was actually written on the wire BEFORE
		// advancing — used by the debug log below. Important because the new
		// wrap rule (255 → 1, never 0) breaks the old "comBusTx.seq - 1u"
		// trick: after a real wrap, seq==1 and that expression would print
		// 0, which is misleading since value 0 is RESERVED for handshake
		// frames (see combus_handshake.h) and must NEVER appear in a control
		// frame log line.
	const uint8_t seqSent = comBusTx.seq;

		// Advance seq counter. The control-frame range is 1..255 — value 0 is
		// reserved for future handshake frames (see combus_handshake.h) and must
		// never appear in a normal frame on the wire. Wrap from 255 → 1.
	comBusTx.seq++;
	if (comBusTx.seq == 0u) { comBusTx.seq = 1u; }

	output_log_dbg("[COMBUS_TX] seq=%u  len=%u  rl=%d  flags=0x%02X\n",
	               (unsigned)seqSent,
	               (unsigned)frameLen,
	               (int)bus->runLevel,
	               (unsigned)(failSafe ? COMBUS_FLAG_FAILSAFE : 0u));
}



// EOF combus_tx.cpp
