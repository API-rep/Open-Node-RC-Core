/******************************************************************************
 * @file remote_link_fallback_chain.cpp
 * @brief REMOTE_LINK_LOST aggregator chain — mirrors failsafe_chain.cpp.
 *
 * @details This chain implements the "any remote input source alive"
 *   aggregator (chantier 12.5 new design).  It mirrors the
 *   failsafe_chain.cpp structure exactly:
 *
 *   1. `cb_reset_fn`  (always)        — forces pipeline `value = 0`
 *                                       at cycle start.  Guarantees that
 *                                       REMOTE_LINK_LOST output starts
 *                                       each cycle in a known state
 *                                       (= healthy).
 *
 *   2. `cb_or_fn` for each contributor  — OR-guard on a contributor
 *                                       channel.  If the contributor
 *                                       reports fault (true), the pipeline
 *                                       latches to 1; the contributor
 *                                       channel is then consumed (reset
 *                                       back to its failsafe default).
 *
 *   Contributors (optional, gated by build flags):
 *     - PS4_DS4_BT_LINK_LOST — PS4 DualShock Bluetooth link health
 *       (active when INPUT_PS4_DS4_BT is defined).
 *     - UART_LINK_LOST       — any UART-backed ComBus transport link
 *       health (active when COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART
 *       is defined).
 *
 *   The chain has primary `outCh` = DigitalComBusID::REMOTE_LINK_LOST so
 *   the runner commits the latched `value` to the top-level ComBus
 *   channel.  Consumers (FSM, runlevel handler, dashboards) then read
 *   REMOTE_LINK_LOST directly on the bus to trigger their reaction.
 *
 *   The IDLE-on-loss reaction (cb_runlevel_once_fn watching
 *   REMOTE_LINK_LOST) is mounted in the RUNLEVEL chain, not here.
 *
 *   This whole module is gated by `-D HAS_REMOTE_LINK_LOST_FALLBACK`
 *   (mirroring the umbrella pattern of failsafe.cb / failsafe_chain.cpp).
 *   Without the flag, the chain is empty (count == 0) and the
 *   `remote_link_fallback_update()` façade is a no-op.
 *
 *   Reference: failsafe_module.md §12.5 (new design — chantier 12.5).
 *****************************************************************************/

#include "remote_link_fallback_chain.h"

#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)

#include <core/system/combus/processors/base/cb_reset.h>   // cb_reset_fn
#include <core/system/combus/processors/logic/cb_or.h>      // cb_or_fn
#include <core/config/machines/combus_types.h>              // DigitalComBusID::PS4_DS4_BT_LINK_LOST, UART_LINK_LOST, REMOTE_LINK_LOST


// =============================================================================
// 1. PROC REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief Static CbProc table — reset + OR-guard contributors.
 *
 * @details Reset is the first entry (always). Contributors are appended
 *   when their respective build flag is set.  Each contributor uses
 *   `cb_or_fn` with inCh == outCh (consume + guard pattern, same as
 *   failsafe_chain.cpp).
 *
 *   Important: the OR-guard pattern REPLACES the open-drain `isNotDrived`
 *   flag.  Each contributor is now independent (no shared state), with
 *   its own failsafe-by-default contract: it stays at `true` (fault)
 *   until its owner writes `false` (healthy) this cycle, then the chain
 *   consumes it via the OR-guard reset.
 */
static CbProc kRemoteLinkFallbackProcs[] = {
    // --- 0. Reset (always present) -----------------------------------------
    // Forces pipeline value = 0 (= healthy) at cycle start.  This is the
    // foundation of the failsafe-by-default contract: the aggregator
    // starts healthy, contributors OR-fault back to 1 if needed.
    {
        .name    = "remote_link_fallback_reset",
        .fn      = cb_reset_fn,
    },

#if defined(INPUT_PS4_DS4_BT)
    // --- 1. PS4 DualShock Bluetooth link contributor (optional) -----------
    // Reads PS4_DS4_BT_LINK_LOST (proof-of-life from the PS4_BT input
    // backend, written each cycle by input_update.cpp).  If the PS4
    // controller is not connected / not refreshing the bus, the channel
    // stays at its failsafe default (true = lost); this OR-guard latches
    // the pipeline value to 1 and consumes the channel back to true.
    {
        .name    = "remote_link_or_ps4_ds4_bt",
        .inCh    = DigitalComBusID::PS4_DS4_BT_LINK_LOST,
        .outCh   = DigitalComBusID::PS4_DS4_BT_LINK_LOST,
        .fn      = cb_or_fn,
    },
#endif

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)
    // --- 2. UART-backed ComBus transport link contributor (optional) ------
    // Reads UART_LINK_LOST (proof-of-life from any UART ComBus transport
    // polled by combus_rx_is_alive() at a higher level — sound node side
    // for RX, machine TX-side via combus_tx watchdog if needed).  If no
    // UART owner refreshed the bus within timeout, the channel stays at
    // its failsafe default (true = lost); this OR-guard latches the
    // pipeline value to 1 and consumes it.
    //
    // Multi-instance: all UART owners write to the SAME channel with
    // `direction: none` (SYSTEM scope) — any board's UART that is down
    // contributes to the aggregated fault.
    {
        .name    = "remote_link_or_uart",
        .inCh    = DigitalComBusID::UART_LINK_LOST,
        .outCh   = DigitalComBusID::UART_LINK_LOST,
        .fn      = cb_or_fn,
    },
#endif

    // Add new contributors here with their own #if defined(...) blocks.
    // Each must follow the same shape: inCh == outCh, fn = cb_or_fn,
    // never claims the chain (the per-proc write-back is enough).
};

/// @brief Static proc count — equals the array length.
static const uint8_t kRemoteLinkFallbackProcsCount =
    sizeof(kRemoteLinkFallbackProcs) / sizeof(kRemoteLinkFallbackProcs[0]);


// =============================================================================
// 2. CHAIN REGISTRY — DEFINITION
// =============================================================================

/**
 * @brief REMOTE_LINK_LOST aggregator chain table — single chain.
 *
 * @details The chain has no primary `inCh` (the reset proc seeds the
 *   pipeline value to 0).  The primary `outCh` is
 *   `DigitalComBusID::REMOTE_LINK_LOST` — the runner commits the latched
 *   pipeline value to the top-level channel after all procs.
 */
CbChain kRemoteLinkFallbackChain[] = {
    {
        .name      = "remote_link_fallback_main",
        .inCh      = std::nullopt,                          // no seed (reset proc)
        .outCh     = DigitalComBusID::REMOTE_LINK_LOST,     // top-level latch
        .procs     = kRemoteLinkFallbackProcs,
        .procCount = kRemoteLinkFallbackProcsCount,
    },
};

/// @brief Static chain count — equals the array length.
const uint8_t kRemoteLinkFallbackChainCount =
    sizeof(kRemoteLinkFallbackChain) / sizeof(kRemoteLinkFallbackChain[0]);


// =============================================================================
// 3. FAÇADE — single-chain update
// =============================================================================

#include <core/system/combus/processors/proc_chain.h>   // proc_chain_update()

void remote_link_fallback_update(ComBus& bus) {
    proc_chain_update(kRemoteLinkFallbackChain, kRemoteLinkFallbackChainCount, bus);
}

#else  // HAS_REMOTE_LINK_LOST_FALLBACK

// Fallback disabled: expose a no-op façade + empty chain table.  Callers
// (sys_manager.cpp) can still invoke remote_link_fallback_update() —
// it just returns immediately without touching the bus.  The
// REMOTE_LINK_LOST channel stays at its failsafe default (high = lost),
// which is the correct behaviour for projects that opted out of the
// fallback (autonomous machines, or projects that handle link-loss at
// a different level).
void remote_link_fallback_update(ComBus& /*bus*/) {
    // no-op
}
const uint8_t kRemoteLinkFallbackChainCount = 0u;

#endif  // HAS_REMOTE_LINK_LOST_FALLBACK

// EOF remote_link_fallback_chain.cpp