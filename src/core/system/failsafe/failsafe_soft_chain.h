/******************************************************************************
 * @file  failsafe_soft_chain.h
 * @brief The soft failsafe chain: publishes FAILSAFE_SOFT, which leads to IDLE.
 *
 * @details Same shape as the hard chain (failsafe_chain.h): a reset, then one
 *   cb_or_fn per contributor, each under its own HAS_*_FAILSAFE flag.  Its faults are
 *   light, today losses of command: the local remote no longer answers, or a link to
 *   another node is down.  The reaction, IDLE on the rising edge, is in the runlevel
 *   chain of the machine.  See failsafe_soft.md.
 *
 *   Without HAS_FAILSAFE, failsafe_soft_update() is a no-op and
 *   kFailsafeSoftChainCount is 0; kFailsafeSoftChain is not defined, so nothing may
 *   read it.
 *****************************************************************************/
#pragma once

#include <cstdint>
#include <struct/combus_proc_struct.h>   // CbChain



// =============================================================================
// 1. TABLES
// =============================================================================

	/// The chain: no input, the reset seeds the value; FAILSAFE_SOFT as output.
extern CbChain kFailsafeSoftChain[];

	/// Number of chains in kFailsafeSoftChain[]: 1, or 0 without HAS_FAILSAFE.
extern const uint8_t kFailsafeSoftChainCount;



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Run the soft chain once: publishes FAILSAFE_SOFT.
 *
 * @details Called by sys_manager_update() after the hard chain and before the state
 *   machine.  No-op without HAS_FAILSAFE.
 *
 * @param bus  ComBus of the node.
 */

void failsafe_soft_update(ComBus& bus);

// EOF failsafe_soft_chain.h
