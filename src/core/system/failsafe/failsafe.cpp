/******************************************************************************
 * @file failsafe.cpp
 * @brief Central Failsafe module — implementation.
 *
 * @details Defines the global pivot `failsafeBus` and the two public
 *   functions `failsafe_init()` and `failsafe_update()`.
 *
 *   At step 2 (WIP §12.2):
 *     - `failsafeBus` is zero-initialised (`active = false`);
 *     - `failsafe_init()` clears the pivot at boot;
 *     - `failsafe_update()` iterates `kFailsafeChain[]` and runs each
 *       CbProc in order. The reset processor (first entry) clears
 *       the pivot; subsequent contributor processors (added in
 *       steps 12.5+) will run after the reset and may set the pivot
 *       back to `true` when they detect a fault.
 *
 *   The reset is performed **exclusively** by the reset processor.
 *   `failsafe_update()` never resets the pivot inline.
 *****************************************************************************/

#include "failsafe.h"

#include "failsafe_chain.h"                     // kFailsafeChain, kFailsafeChainCount
#include <core/system/combus/processors/proc_chain.h>  // proc_chain_update()


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
 * @brief Failsafe orchestrator — runs every CbChain registered in
 *   `kFailsafeChain[]` via the standard `proc_chain_update()` runner.
 *
 * @details Delegates to the shared ComBus chain runner, which already
 *   implements the full contract:
 *     - seed `value` from `ch.inCh` (none here → 0);
 *     - for each proc: read `proc->inCh` into `proc->inValue`,
 *       call `proc.fn(&proc, value, claimed)`, commit `proc->outValue`
 *       to `proc->outCh` (using `combus_set_digital` layer-checked);
 *     - commit final `value` to `ch.outCh` (none here → no-op).
 *
 *   WIP invariant: every registered processor is called every cycle
 *   (the standard runner skips procs when `claimed = true`; Failsafe
 *   procs MUST therefore never claim — see proc_failsafe_reset.h /
 *   proc_failsafe_vbat.h).  This guarantees that every contributor
 *   sees the freshly-reset pivot and is itself responsible for
 *   resetting its `FAILSAFE_X` channel.
 *
 *   WIP §6 invariant: every source module update must be finished
 *   **before** `failsafe_update()` is called. This ordering is
 *   enforced by `sys_manager_update` and does not depend on the
 *   Failsafe module itself.
 *
 * @param bus  Shared ComBus — forwarded to the standard chain runner.
 */
void failsafe_update(ComBus& bus)
{
    proc_chain_update(kFailsafeChain, kFailsafeChainCount, bus);
}

// EOF failsafe.cpp
