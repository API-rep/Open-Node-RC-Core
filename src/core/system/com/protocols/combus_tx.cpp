/******************************************************************************
 * @file combus_tx.cpp
 * @brief ComBus transmitter — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_tx.h"

#include <core/combus/combus_frame.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

struct CombusTxState {
	NodeCom* com       = nullptr;  ///< active transport interface
	ComBusFrameCfg  cfg       = {};        ///< static layout descriptor (envId, nAnalog, nDigital)
	uint8_t         seq       = 0u;        ///< rolling frame sequence counter (0–255)
	uint32_t        lastTxMs  = 0u;        ///< timestamp of last transmitted frame (ms)
	uint32_t        periodMs  = 0u;        ///< transmit period derived from txHz (0 = uninit)
};

static CombusTxState s_tx;


// =============================================================================
// 2. INITIALIZATION
// =============================================================================

void combus_tx_init( NodeCom*       com,
                     ComBusFrameCfg cfg,
                     uint32_t       txHz ) {

	if (!com || txHz == 0u) { return; }

	s_tx.com      = com;
	s_tx.cfg      = cfg;
	s_tx.periodMs = 1000u / txHz;

	sys_log_info("[COMBUS_TX] init — transport='%s'  rate=%uHz  A%u+D%u\n",
	             com->name, txHz,
	             (unsigned)cfg.nAnalog, (unsigned)cfg.nDigital);
}


// =============================================================================
// 3. TRANSMIT UPDATE
// =============================================================================

void combus_tx_update(const ComBus* bus, bool failSafe) {

		// --- Guard: init not done or invalid bus ---
	if (!s_tx.com || s_tx.periodMs == 0u || !bus) { return; }

		// --- Timer gate ---
	uint32_t now = millis();
	if ((now - s_tx.lastTxMs) < s_tx.periodMs) { return; }
	s_tx.lastTxMs = now;

		// --- Encode ---
	static uint8_t frame[255u];
	uint8_t frameLen = combus_frame_encode(
	    s_tx.cfg,
	    frame,
	    bus,
	    s_tx.seq,
	    failSafe
	);

	if (frameLen == 0u) { return; }

		// --- Send via transport ---
	s_tx.com->write(s_tx.com->ctx, frame, frameLen);
	s_tx.seq++;

	output_log_dbg("[COMBUS_TX] seq=%u  len=%u  rl=%d  flags=0x%02X\n",
	               (unsigned)(s_tx.seq - 1u),
	               (unsigned)frameLen,
	               (int)bus->runLevel,
	               (unsigned)(failSafe ? COMBUS_FLAG_FAILSAFE : 0u));
}

// EOF combus_tx.cpp
