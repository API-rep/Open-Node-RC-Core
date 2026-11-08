/******************************************************************************
 * @file  failsafe_chain.cpp
 * @brief The hard failsafe chain: its processors, and the chain that runs them.
 *
 * @details Two generic processors, nothing specific to the failsafe:
 *   - cb_reset_fn, always first: every contributor sees a value reset this cycle;
 *   - cb_or_fn, one per contributor, inCh == outCh: it reads the contributor, sets
 *     the value to 1 if it is in fault, and puts the contributor back to fault
 *     (consume and guard).
 *
 *   A new contributor (temperature...) is added after the others, under its own
 *   HAS_*_FAILSAFE flag, always with cb_or_fn.
 *****************************************************************************/

#include "failsafe_chain.h"

#include <core/system/combus/processors/base/cb_reset.h>   // cb_reset_fn
#include <core/system/combus/processors/logic/cb_or.h>      // cb_or_fn
#include <core/config/machines/combus_types.h>              // DigitalComBusID



// =============================================================================
// 1. PROCESSORS
// =============================================================================

CbProc kFailsafeProcs[] = {
		// 0. Reset, always first: no channel, the value back to 0
	{
		.name    = "failsafe_reset",
		.fn      = cb_reset_fn,
	},

#if defined(HAS_VBAT_FAILSAFE)
		// 1. Low battery, written every cycle by vbat_update()
	{
		.name    = "failsafe_or_vbat",
		.inCh    = DigitalComBusID::VBAT_FAIL,
		.outCh   = DigitalComBusID::VBAT_FAIL,
		.fn      = cb_or_fn,
	},
#endif

#if defined(HAS_BOARD_LINK_FAILSAFE)
		// 2. Link between boards lost, or never seen after its startup grace;
		//    written every cycle by com_rx_update(), false without such a link
	{
		.name    = "failsafe_or_com_link",
		.inCh    = DigitalComBusID::BOARD_LINK_FAIL,
		.outCh   = DigitalComBusID::BOARD_LINK_FAIL,
		.fn      = cb_or_fn,
	},
#endif
};

const uint8_t kFailsafeProcsCount = sizeof(kFailsafeProcs) / sizeof(kFailsafeProcs[0]);



// =============================================================================
// 2. CHAIN
// =============================================================================

CbChain kFailsafeChain[] = {
	{
		.name      = "failsafe_main",
		.inCh      = std::nullopt,                      // no seed: the reset sets the value
		.outCh     = DigitalComBusID::FAILSAFE_HARD,
		.procs     = kFailsafeProcs,
		.procCount = kFailsafeProcsCount,
	},
};

const uint8_t kFailsafeChainCount = sizeof(kFailsafeChain) / sizeof(kFailsafeChain[0]);

// EOF failsafe_chain.cpp
