/******************************************************************************
 * @file failsafe.cpp
 * @brief Failsafe module — central orchestration (A16.3 transitional).
 *
 * @details At A16.3:
 *   - `failsafe_init()` is a **no-op** (the per-cycle reset is now
 *     performed by the `cb_reset_fn` processor in the chain).
 *   - `failsafe_update(ComBus& bus)` runs `kFailsafeChain[]` via the
 *     standard `proc_chain_update()` runner.
 *
 *   No global pivot (`failsafeBus`) is maintained here — the chain
 *   writes its aggregated result directly to the `DigitalComBusID::FAILSAFE`
 *   ComBus channel.  Consumers read `FAILSAFE` on the bus.
 *****************************************************************************/

#include "failsafe.h"

#include "failsafe_chain.h"                        // kFailsafeChain, kFailsafeChainCount
#include <core/system/combus/processors/proc_chain.h>  // proc_chain_update()


// =============================================================================
// 1. PUBLIC API — IMPLEMENTATION
// =============================================================================

/**
 * @brief No-op init.
 *
 * @details Kept for API compatibility.  Will be removed when the chain
 *   refactor is complete.
 */
void failsafe_init()
{
    // No-op — the per-cycle reset is performed by cb_reset_fn in the
    // chain, so there is no global pivot to clear at boot.
}

/**
 * @brief Failsafe orchestrator — runs every CbChain registered in
 *   `kFailsafeChain[]` via the standard `proc_chain_update()` runner.
 *
 * @details The standard runner already implements the full contract:
 *   seed `value` from `ch.inCh`, iterate procs (read `inCh` into
 *   `inValue`, call `fn`, commit `outValue` to `outCh`), commit final
 *   `value` to `ch.outCh`.
 *
 * @param bus  Shared ComBus — forwarded to the standard chain runner.
 */
void failsafe_update(ComBus& bus)
{
    proc_chain_update(kFailsafeChain, kFailsafeChainCount, bus);
}

// EOF failsafe.cpp
