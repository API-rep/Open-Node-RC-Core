/******************************************************************************
 * @file failsafe_chain.cpp
 * @brief Failsafe main chain — registry definitions.
 *
 * @details At step 2 (WIP §12.2), the Failsafe main chain contains:
 *   - exactly one CbProc (the reset processor);
 *   - exactly one CbChain wrapping that CbProc.
 *
 *   Contributor CbChains will be added in upcoming steps. They will
 *   be appended to `kFailsafeChain[]` while the reset entry remains
 *   the first one to guarantee that every contributor sees a freshly
 *   reset pivot.
 *****************************************************************************/

#include "failsafe_chain.h"

#include "proc_failsafe_reset.h"  // proc_failsafe_reset_fn


// =============================================================================
// 1. PROC REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Static CbProc table — step 2 holds only the reset processor.
 *
 * @details The processor has no channel input, no channel output, no
 *   cfg and no state. Its sole effect is to clear `failsafeBus.active`.
 *   The `name` field is used for debug / dashboard rendering.
 */
CbProc kFailsafeProcs[] = {
    {
        .name    = "failsafe_reset",
        .fn      = proc_failsafe_reset_fn,
        // inCh, inValue, outCh, outValue, cfg, dynCfg, state default to
        // nullopt / 0 / nullptr — none are needed by the reset processor.
    },
};

/// @brief Static proc count — equals the array length.
const uint8_t kFailsafeProcsCount = sizeof(kFailsafeProcs) / sizeof(kFailsafeProcs[0]);


// =============================================================================
// 2. CHAIN REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Failsafe CbChain table — step 2 holds exactly one entry.
 *
 * @details The chain has no primary `inCh`/`outCh` because the reset
 *   processor does not consume or produce any ComBus channel. The
 *   chain thus relies entirely on its internal `procs[]` array.
 */
CbChain kFailsafeChain[] = {
    {
        .name      = "failsafe_main",
        .procs     = kFailsafeProcs,
        .procCount = kFailsafeProcsCount,
        // inCh / outCh default to nullopt — no primary I/O at step 2.
    },
};

/// @brief Static chain count — equals the array length.
const uint8_t kFailsafeChainCount = sizeof(kFailsafeChain) / sizeof(kFailsafeChain[0]);

// EOF failsafe_chain.cpp
