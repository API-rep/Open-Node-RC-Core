/******************************************************************************
 * @file failsafe_chain.cpp
 * @brief Failsafe main chain — registry definitions.
 *
 * @details At A16.2 (WIP §12.5), the Failsafe main chain contains:
 *   - the reset processor (always present) — clears `failsafeBus.active`;
 *   - the VBAT contributor (when HAS_VBAT_FAILSAFE is set) — aggregates
 *     `DigitalComBusID::FAILSAFE_VBAT` into the pivot and resets the
 *     sub-combus to fault.
 *
 *   Future contributors (combus link, temperature, …) will be appended
 *   to `kFailsafeProcs[]` after the reset, each under its own
 *   `#if defined(HAS_XXX_FAILSAFE)` block.
 *
 *   The reset entry is always the FIRST one in the table so that every
 *   contributor sees a freshly-reset pivot (WIP §6 / §7).
 *****************************************************************************/

#include "failsafe_chain.h"

#include "proc_failsafe_reset.h"  // proc_failsafe_reset_fn
#include "proc_failsafe_vbat.h"   // proc_failsafe_vbat_fn  (HAS_VBAT_FAILSAFE only)
#include <core/config/machines/combus_types.h>  // DigitalComBusID::FAILSAFE_VBAT


// =============================================================================
// 1. PROC REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Static CbProc table — reset + conditional contributors.
 *
 * @details The reset processor is the first entry (always). The VBAT
 *   contributor is appended when HAS_VBAT_FAILSAFE is defined. New
 *   contributors should follow the same pattern.
 */
CbProc kFailsafeProcs[] = {
    // --- 0. Reset (always present) -----------------------------------------
    // No inCh / outCh — resets the pivot to false at cycle start.
    {
        .name    = "failsafe_reset",
        .fn      = proc_failsafe_reset_fn,
        // inCh, inValue, outCh, outValue, cfg, dynCfg, state default to
        // nullopt / 0 / nullptr — none are needed by the reset processor.
    },

#if defined(HAS_VBAT_FAILSAFE)
    // --- 1. VBAT contributor (optional) ------------------------------------
    // Reads FAILSAFE_VBAT (proof-of-life from vbat_update). If the cell
    // voltage is below cutoff, FAILSAFE_VBAT is true; this contributor
    // latches the central pivot (`failsafeBus.active = true`) and
    // resets FAILSAFE_VBAT back to fault.
    {
        .name    = "failsafe_vbat",
        .inCh    = DigitalComBusID::FAILSAFE_VBAT,
        .outCh   = DigitalComBusID::FAILSAFE_VBAT,
        .fn      = proc_failsafe_vbat_fn,
    },
#endif

    // Add new contributors here with their own #if defined(HAS_XXX_FAILSAFE)
    // blocks.  Each must follow the same shape: inCh == outCh, fn never
    // claims the chain, inCh is the contributor's sub-combus.
};

/// @brief Static proc count — equals the array length.
const uint8_t kFailsafeProcsCount = sizeof(kFailsafeProcs) / sizeof(kFailsafeProcs[0]);


// =============================================================================
// 2. CHAIN REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Failsafe CbChain table — single chain wrapping the proc array.
 *
 * @details The chain has no primary `inCh`/`outCh` because no Failsafe
 *   proc uses primary I/O. All sub-combus I/O goes through the
 *   per-proc `inCh` / `outCh` (injected by the standard runner). The
 *   chain thus relies entirely on its internal `procs[]` array.
 */
CbChain kFailsafeChain[] = {
    {
        .name      = "failsafe_main",
        .procs     = kFailsafeProcs,
        .procCount = kFailsafeProcsCount,
        // inCh / outCh default to nullopt — no primary I/O.
    },
};

/// @brief Static chain count — equals the array length.
const uint8_t kFailsafeChainCount = sizeof(kFailsafeChain) / sizeof(kFailsafeChain[0]);

// EOF failsafe_chain.cpp
