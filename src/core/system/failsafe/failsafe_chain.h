/******************************************************************************
 * @file failsafe_chain.h
 * @brief Failsafe ComBus chain — central CbChain declaration.
 *
 * @details At A16.3, the Failsafe module owns a single CbChain wrapping
 *   a single CbProc table.  The proc table contains:
 *     - `procs[0]` = `cb_reset_fn`  (always)  — clears the pipeline
 *                                                 value (WIP §7 latch).
 *     - `procs[1..N]` = `cb_or_fn` contributors — aggregated
 *       conditionally under `#if defined(HAS_XXX_FAILSAFE)` blocks.
 *
 *   The single chain writes the latched pipeline value to
 *   `DigitalComBusID::FAILSAFE` via its primary `outCh`.  Consumers
 *   (e.g. `main.cpp`) read this channel directly on the bus.
 *
 *   The `kFailsafeProcsCount` is computed at link time
 *   (`sizeof(kFailsafeProcs) / sizeof(kFailsafeProcs[0])`) and grows
 *   as new contributors are added.
 *
 *   The `kFailsafeChainCount` is always 1 (single chain wrapping the
 *   full proc table).
 *
 *   WIP §6 invariant: every contributor must run every cycle
 *   (the standard `proc_chain_update` runner skips procs when
 *   `claimed = true`; Failsafe procs MUST therefore never claim —
 *   see `cb_or.h`).
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbChain


// =============================================================================
// 1. PROC REGISTRY
// =============================================================================

/**
 * @brief Static CbProc table — reset + conditional OR-guard contributors.
 *
 * @details Reset (`cb_reset_fn`) is always at index 0.  Contributors
 *   (`cb_or_fn`) are appended under their respective
 *   `#if defined(HAS_XXX_FAILSAFE)` blocks.  See `failsafe_chain.cpp`
 *   for the current table.
 */
extern CbProc kFailsafeProcs[];

/// @brief Number of CbProc registered in `kFailsafeProcs[]` (computed at link time).
extern const uint8_t kFailsafeProcsCount;


// =============================================================================
// 2. CHAIN REGISTRY
// =============================================================================

/**
 * @brief Failsafe CbChain table — single chain wrapping the proc array.
 *
 * @details The chain has no primary `inCh` (the reset proc seeds the
 *   pipeline value to 0).  The primary `outCh` is
 *   `DigitalComBusID::FAILSAFE` — the standard runner commits the
 *   latched pipeline value to the top-level ComBus channel after all
 *   procs.
 */
extern CbChain kFailsafeChain[];

/// @brief Number of CbChain registered in `kFailsafeChain[]` (always 1).
extern const uint8_t kFailsafeChainCount;

// EOF failsafe_chain.h
