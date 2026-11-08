/******************************************************************************
 * @file  failsafe.cpp
 * @brief Failsafe module: the hard chain, which leads to HALT.
 *
 * @details No state here: the chain writes its result to FAILSAFE_HARD, and the
 *   consumers read it on the bus.
 *****************************************************************************/

#include "failsafe.h"

#include "failsafe_chain.h"                             // kFailsafeChain, kFailsafeChainCount
#include <core/system/combus/processors/proc_chain.h>   // proc_chain_update()



// =============================================================================
// 1. API
// =============================================================================

	/// The standard runner: reset, contributors, then FAILSAFE_HARD.
void failsafe_update(ComBus& bus)
{
	proc_chain_update(kFailsafeChain, kFailsafeChainCount, bus);
}

// EOF failsafe.cpp
