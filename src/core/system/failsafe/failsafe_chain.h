/******************************************************************************
 * @file  failsafe_chain.h
 * @brief The hard failsafe chain: its processors, and the chain that runs them.
 *
 * @details One chain, with no input and FAILSAFE_HARD as output:
 *   - kFailsafeProcs[0]: cb_reset_fn, the value back to 0 (healthy) every cycle;
 *   - then one cb_or_fn per contributor, each under its own HAS_*_FAILSAFE flag: a
 *     contributor in fault sets the value to 1.
 *
 *   No processor ever claims the chain: every contributor must run every cycle
 *   (see cb_or.h).
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>   // CbProc, CbChain



// =============================================================================
// 1. TABLES
// =============================================================================

	/// The reset, then one OR per contributor (failsafe_chain.cpp).
extern CbProc kFailsafeProcs[];

	/// Number of entries in kFailsafeProcs[].
extern const uint8_t kFailsafeProcsCount;

	/// The chain: no input, the reset seeds the value; FAILSAFE_HARD as output.
extern CbChain kFailsafeChain[];

	/// Number of chains in kFailsafeChain[]: 1.
extern const uint8_t kFailsafeChainCount;

// EOF failsafe_chain.h
