/******************************************************************************
 * @file failsafe_chain.h
 * @brief Failsafe main chain — central CbChain declaration.
 *
 * @details Centralises the declaration of the `kFailsafeChain[]`
 *   table that feeds the `failsafe_update()` orchestrator.
 *
 *   At step 2 (WIP §12.2), the chain contains exactly one CbChain
 *   holding exactly one CbProc: the reset processor. Contributor
 *   CbChains (input link, VBAT, ComBus link, …) will be added in
 *   the upcoming steps.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbChain


// =============================================================================
// 1. PROC REGISTRY
// =============================================================================

/**
 * @brief Static CbProc table backing the Failsafe main chain.
 *
 * @details At step 2, the table holds exactly one entry — the reset
 *   processor. Future contributors will be appended to dedicated
 *   CbChain entries; the main chain's CbProc count stays at 1.
 */
extern CbProc kFailsafeProcs[];

/// @brief Number of CbProc registered in `kFailsafeProcs[]` (step 2 = 1).
extern const uint8_t kFailsafeProcsCount;


// =============================================================================
// 2. CHAIN REGISTRY
// =============================================================================

/**
 * @brief Failsafe CbChain table — execution order = table order.
 *
 * @details At step 2, the table contains exactly one CbChain. The
 *   order of processors within the chain is fixed:
 *     - `procs[0]` = reset (clears `failsafeBus.active`).
 *   Contributor chains will be appended to the table in upcoming
 *   steps (12.5+); each will run after the reset, preserving the
 *   invariant that **every contributor sees a freshly-reset pivot**.
 */
extern CbChain kFailsafeChain[];

/// @brief Number of CbChain registered in `kFailsafeChain[]` (step 2 = 1).
extern const uint8_t kFailsafeChainCount;

// EOF failsafe_chain.h
