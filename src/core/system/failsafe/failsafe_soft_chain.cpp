/******************************************************************************
 * @file  failsafe_soft_chain.cpp
 * @brief The soft failsafe chain: publishes FAILSAFE_SOFT, which leads to IDLE.
 *
 * @details The same two generic processors as the hard chain:
 *   - cb_reset_fn, always first: the value back to 0 (healthy) every cycle;
 *   - cb_or_fn, one per contributor, inCh == outCh: consume and guard.
 *
 *   Each contributor is independent and fails safe by default: it stays true (fault)
 *   until its source writes false this cycle, and the chain puts it back to true
 *   after reading it.
 *
 *   The whole module needs HAS_FAILSAFE; without it, only a no-op update and a count
 *   of 0 remain.
 *****************************************************************************/

#include "failsafe_soft_chain.h"

#if defined(HAS_FAILSAFE)

#include <core/system/combus/processors/base/cb_reset.h>   // cb_reset_fn
#include <core/system/combus/processors/logic/cb_or.h>      // cb_or_fn
#include <core/config/machines/combus_types.h>              // DigitalComBusID



// =============================================================================
// 1. PROCESSORS
// =============================================================================

static CbProc kFailsafeSoftProcs[] = {
		// 0. Reset, always first: no channel, the value back to 0
	{
		.name    = "failsafe_soft_reset",
		.fn      = cb_reset_fn,
	},

#if defined(HAS_LOCAL_REMOTE_FAILSAFE)
		// 1. Local remote lost, written every cycle by input_update() while the
		//    input backend (PS4 Bluetooth today) is connected
	{
		.name    = "failsafe_soft_or_local_remote",
		.inCh    = DigitalComBusID::LOCAL_REMOTE_FAIL,
		.outCh   = DigitalComBusID::LOCAL_REMOTE_FAIL,
		.fn      = cb_or_fn,
	},
#endif

#if defined(HAS_NODE_LINK_FAILSAFE)
		// 2. Link to another node not alive, never seen included; written every
		//    cycle by com_rx_update(), false without such a link
	{
		.name    = "failsafe_soft_or_node_link",
		.inCh    = DigitalComBusID::NODE_LINK_FAIL,
		.outCh   = DigitalComBusID::NODE_LINK_FAIL,
		.fn      = cb_or_fn,
	},
#endif
};

static const uint8_t kFailsafeSoftProcsCount =
	sizeof(kFailsafeSoftProcs) / sizeof(kFailsafeSoftProcs[0]);



// =============================================================================
// 2. CHAIN
// =============================================================================

CbChain kFailsafeSoftChain[] = {
	{
		.name      = "failsafe_soft_main",
		.inCh      = std::nullopt,                      // no seed: the reset sets the value
		.outCh     = DigitalComBusID::FAILSAFE_SOFT,
		.procs     = kFailsafeSoftProcs,
		.procCount = kFailsafeSoftProcsCount,
	},
};

const uint8_t kFailsafeSoftChainCount =
	sizeof(kFailsafeSoftChain) / sizeof(kFailsafeSoftChain[0]);



// =============================================================================
// 3. API
// =============================================================================

#include <core/system/combus/processors/proc_chain.h>   // proc_chain_update()

	/// The standard runner: reset, contributors, then FAILSAFE_SOFT.
void failsafe_soft_update(ComBus& bus) {
	proc_chain_update(kFailsafeSoftChain, kFailsafeSoftChainCount, bus);
}

#else  // HAS_FAILSAFE

	// No failsafe module: nothing to run, and no FAILSAFE_SOFT channel either.
void failsafe_soft_update(ComBus& /*bus*/) {
}

const uint8_t kFailsafeSoftChainCount = 0u;

#endif  // HAS_FAILSAFE

// EOF failsafe_soft_chain.cpp
