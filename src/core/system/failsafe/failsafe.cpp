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

#include "failsafe_chain.h"  // kFailsafeChain, kFailsafeChainCount


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
 *   `kFailsafeChain[]`.
 *
 * @details At step 2, the chain contains only the reset processor.
 *   A minimal local runner iterates each chain's CbProc array in
 *   order and calls `proc.fn(&proc, value, claimed)` with a local
 *   scratch `value` (no channel seeding — no Failsafe proc reads a
 *   channel at step 2) and a local `claimed` flag (the reset
 *   processor never claims).
 *
 *   This local runner mirrors the contract of `proc_chain_step()`
 *   (`src/core/system/combus/processors/proc_chain.cpp`) but without
 *   the ComBus parameter. When contributor CbChains (steps 12.5+)
 *   require channel I/O, this runner will be replaced by a call to
 *   `proc_chain_update()` and `failsafe_update()` will accept the
 *   shared ComBus.
 *
 *   WIP invariant: every registered processor is called every cycle,
 *   in order, regardless of `claimed`. `claimed` is a state forwarded
 *   to each processor — it is **not** a chain-break mechanism. This
 *   guarantees that every contributor sees the freshly-reset pivot
 *   and is itself responsible for setting its `FAILSAFE_X` channel.
 *
 *   WIP §6 invariant: every source module update must be finished
 *   **before** `failsafe_update()` is called. This ordering is
 *   enforced by `sys_manager_update` and does not depend on the
 *   Failsafe module itself.
 */
void failsafe_update()
{
    for (uint8_t c = 0; c < kFailsafeChainCount; ++c) {
        CbChain& ch = kFailsafeChain[c];

        // Local pipeline — no ComBus seeding at step 2.
        uint16_t value   = 0u;
        bool     claimed = false;

        for (uint8_t p = 0; p < ch.procCount; ++p) {
            CbProc& proc = ch.procs[p];
            if (proc.fn == nullptr) continue;

            // NOTE: every processor is invoked every cycle; `claimed`
            // is forwarded as state but never short-circuits the chain.
            proc.fn(&proc, value, claimed);
        }
    }
}

// EOF failsafe.cpp
