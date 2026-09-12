/******************************************************************************
 * @file failsafe_chain.cpp
 * @brief Failsafe main chain — registry definitions.
 *
 * @details At A16.3, the Failsafe main chain uses two GENERIC CbProcFn
 *   (no Failsafe-specific processor in this module):
 *   - `cb_reset_fn`  (always)        — forces pipeline `value = 0`
 *                                       at cycle start (WIP §7 latch).
 *   - `cb_or_fn`     (HAS_VBAT_*)    — OR-guard on a sub-combus
 *                                       (consume + guard pattern,
 *                                       WIP §5 / §6).
 *
 *   The chain has a primary `outCh` = `DigitalComBusID::FAILSAFE` so
 *   the runner commits the latched `value` to the top-level ComBus
 *   channel. Consumers (e.g. `main.cpp`) then read `FAILSAFE` directly
 *   on the bus to trigger their reaction.
 *
 *   Future contributors (combus link, temperature, …) will be appended
 *   to `kFailsafeProcs[]` after the reset, each under its own
 *   `#if defined(HAS_XXX_FAILSAFE)` block — always with `cb_or_fn`.
 *
 *   The reset entry is always the FIRST one in the table so that every
 *   contributor sees a freshly-reset pipeline value (WIP §6 / §7).
 *****************************************************************************/

#include "failsafe_chain.h"

#include <core/system/combus/processors/base/cb_reset.h>   // cb_reset_fn
#include <core/system/combus/processors/logic/cb_or.h>      // cb_or_fn
#include <core/config/machines/combus_types.h>              // DigitalComBusID::FAILSAFE / FAILSAFE_VBAT


// =============================================================================
// 1. PROC REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Static CbProc table — reset + conditional OR-guard contributors.
 *
 * @details Reset is the first entry (always). VBAT contributor is
 *   appended when HAS_VBAT_FAILSAFE is defined. New contributors
 *   follow the same pattern (use `cb_or_fn` with inCh == outCh).
 */
CbProc kFailsafeProcs[] = {
    // --- 0. Reset (always present) -----------------------------------------
    // No inCh / outCh — forces pipeline value = 0 at cycle start.
    {
        .name    = "failsafe_reset",
        .fn      = cb_reset_fn,
        // inCh, inValue, outCh, outValue, cfg, dynCfg, state default to
        // nullopt / 0 / nullptr — none are needed by the reset processor.
    },

#if defined(HAS_VBAT_FAILSAFE)
    // --- 1. VBAT OR-guard contributor (optional) ---------------------------
    // Reads FAILSAFE_VBAT (proof-of-life from vbat_update). If the cell
    // voltage is below cutoff, FAILSAFE_VBAT is true; this OR-guard
    // latches the pipeline value to 1 (= DigitalComBusID::FAILSAFE fault)
    // and resets FAILSAFE_VBAT back to fault (consume + guard).
    {
        .name    = "failsafe_or_vbat",
        .inCh    = DigitalComBusID::FAILSAFE_VBAT,
        .outCh   = DigitalComBusID::FAILSAFE_VBAT,
        .fn      = cb_or_fn,
    },
#endif

#if defined(HAS_COMBUS_LINK_FAILSAFE)
    // --- 2. ComBus-link OR-guard contributor (FS1, optional) ---------------
    // Reads FAILSAFE_COMBUS_LINK (proof-of-life from sys_manager_update).
    // If no physical input source refreshed the bus this cycle
    // (isNotDrived == true), FAILSAFE_COMBUS_LINK is true; this OR-guard
    // latches the pipeline value to 1 (= DigitalComBusID::FAILSAFE fault)
    // and resets FAILSAFE_COMBUS_LINK back to fault (consume + guard).
    //
    // This contributor replaces the historical `isDrived` open-drain flag
    // on ComBus (see RL0 audit). The semantics are inverted: `true` =
    // fault (no combus update this cycle), `false` = healthy (at least
    // one source refreshed the bus).
    {
        .name    = "failsafe_or_combus_link",
        .inCh    = DigitalComBusID::FAILSAFE_COMBUS_LINK,
        .outCh   = DigitalComBusID::FAILSAFE_COMBUS_LINK,
        .fn      = cb_or_fn,
    },
#endif

    // Add new contributors here with their own #if defined(HAS_XXX_FAILSAFE)
    // blocks.  Each must follow the same shape: inCh == outCh, fn = cb_or_fn,
    // never claims the chain.
};

/// @brief Static proc count — equals the array length.
const uint8_t kFailsafeProcsCount = sizeof(kFailsafeProcs) / sizeof(kFailsafeProcs[0]);


// =============================================================================
// 2. CHAIN REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Failsafe CbChain table — single chain wrapping the proc array.
 *
 * @details The chain has no primary `inCh` (the reset proc seeds the
 *   pipeline value to 0).  The primary `outCh` is
 *   `DigitalComBusID::FAILSAFE` — the runner commits the latched
 *   pipeline value to the top-level channel after all procs.
 */
CbChain kFailsafeChain[] = {
    {
        .name      = "failsafe_main",
        .inCh      = std::nullopt,                          // no seed
        .outCh     = DigitalComBusID::FAILSAFE,             // top-level latch
        .procs     = kFailsafeProcs,
        .procCount = kFailsafeProcsCount,
    },
};

/// @brief Static chain count — equals the array length.
const uint8_t kFailsafeChainCount = sizeof(kFailsafeChain) / sizeof(kFailsafeChain[0]);

// EOF failsafe_chain.cpp
