/******************************************************************************
 * @file failsafe.cpp
 * @brief Central Failsafe module — implementation.
 *
 * @details Defines the global pivot `failsafeBus` and the two public
 *   functions `failsafe_init()` and `failsafe_update()`.
 *
 *   At step 1 (roadmap WIP §12.1):
 *     - `failsafeBus` is zero-initialised (`active = false`);
 *     - `failsafe_init()` is limited to clearing the pivot;
 *     - `failsafe_update()` is deliberately **no-op**: the main
 *       chain is empty, nothing is iterated and no inline reset is
 *       executed. The reset will be introduced at step 2 through
 *       `proc_failsafe_reset`.
 *****************************************************************************/

#include "failsafe.h"

#include "failsafe_chain.h"  // kFailsafeChain, kFailsafeChainCount (noop while 0)


// =============================================================================
// 1. PIVOT — DEFINITION
// =============================================================================

/// @brief Global instance — zero-initialised (`active = false`) by default.
FailsafeBus failsafeBus = {false};


// =============================================================================
// 2. PUBLIC API — IMPLEMENTATION
// =============================================================================

/**
 * @brief Failsafe initialisation — clears the pivot, no ComBus dependency.
 *
 * @details Idempotent: re-clearing the pivot at boot is safe. The
 *   function does not open, read or write any ComBus channel.
 */
void failsafe_init()
{
    failsafeBus.active = false;
}

/**
 * @brief No-op orchestrator at step 1.
 *
 * @details The chain `kFailsafeChain` is empty (count == 0) and its
 *   pointer is `nullptr`; there is therefore **nothing to iterate**.
 *   The pivot is not touched — the reset will be performed
 *   exclusively at step 2 by `proc_failsafe_reset`.
 */
void failsafe_update()
{
    // WIP: deliberately empty at step 1 (WIP §12.1).
    // The pivot will be reset by proc_failsafe_reset at step 2.
}

// EOF failsafe.cpp
