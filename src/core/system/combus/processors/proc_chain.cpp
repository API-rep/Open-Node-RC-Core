/******************************************************************************
 * @file  proc_chain.cpp
 * @brief CbChain dispatcher — init and update implementation.
 *****************************************************************************/

#include "proc_chain.h"

#include <struct/combus_struct.h>              // ComBus
#include <core/system/combus/combus_access.h>  // combus_set_analog, combus_set_digital


// =============================================================================
// 0. INTERNAL HELPERS
// =============================================================================

using ChanOpt = std::optional<std::variant<AnalogComBusID, DigitalComBusID>>;

/** @brief Read a channel variant from the bus into a uint16_t.
 *
 *  @param isDrivedGuard  When true, digital channels return 0 unless isDrived.
 *                        Use true for primary inputs, false for secondary inputs.
 */
static uint16_t cbRead(const ComBus& bus, const ChanOpt& ch, bool isDrivedGuard)
{
    if (!ch.has_value()) return 0u;
    if (std::holds_alternative<AnalogComBusID>(*ch)) {
        return bus.analogBus[static_cast<uint8_t>(std::get<AnalogComBusID>(*ch))].value;
    }
    const uint8_t idx = static_cast<uint8_t>(std::get<DigitalComBusID>(*ch));
    if (isDrivedGuard && !bus.digitalBus[idx].isDrived) return 0u;
    return bus.digitalBus[idx].value ? 1u : 0u;
}

/** @brief Write a uint16_t value to a channel variant on the bus. */
static void cbWrite(ComBus& bus, const ChanOpt& ch, uint16_t value, ChanOwner owner)
{
    if (!ch.has_value()) return;
    if (std::holds_alternative<AnalogComBusID>(*ch)) {
        combus_set_analog(bus, std::get<AnalogComBusID>(*ch), value, owner);
    } else {
        combus_set_digital(bus, std::get<DigitalComBusID>(*ch), value != 0u, owner);
    }
}


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Placeholder lifecycle init for the CbChain array.
 *
 * @details Kept as a symmetric counterpart to dc_dev_init() / srv_dev_init().
 *   Each CbProcFn self-inits on first update call via zero-state detection.
 */
void proc_chain_init(CbChain* /*channels*/, uint8_t /*count*/)
{
    // Stages self-init on first update call (zero-state detection).
}


/**
 * @brief Process a single CbChain — dispatch all processors.
 *
 * @details Sequence:
 *   1. Value starts at 0 — the first proc (`cb_in_fn`) seeds it.
 *   2. Proc loop — all procs, in order:
 *        a. Inject input: `proc.inValue` ← bus[proc.inCh].
 *        b. Skip when `claimed = true` — EXCEPT the last proc, which always
 *           runs to allow `cb_out_fn` to commit the final value after a bypass.
 *        c. Call `proc.fn(&proc, value, claimed, ch.chainOwner)`.
 *        d. Commit proc output: bus[proc.outCh] ← proc.outValue.
 *
 * @param ch   Channel descriptor (procs, chainOwner).
 * @param bus  Shared ComBus for this cycle.
 */
void proc_chain_step(CbChain& ch, ComBus& bus)
{
    // --- 1. Init pipeline ----------------------------------------------------
    uint16_t value   = 0u;
    bool     claimed = false;

    // --- 2. Process chain ----------------------------------------------------
    for (uint8_t p = 0; p < ch.procCount; ++p) {
        CbProc& proc = ch.procs[p];
        if (proc.fn == nullptr) continue;

        //  a. Inject input from proc.inCh.
        proc.inValue = cbRead(bus, proc.inCh, /*isDrivedGuard=*/false);

        //  b. Skip when claimed — last proc always runs (cb_out_fn commits).
        const bool isLast = (p == ch.procCount - 1u);
        if (claimed && !isLast) continue;

        //  c. Call proc fn (no bus access inside fn).
        proc.fn(&proc, value, claimed, ch.chainOwner);

        //  d. Commit proc output.
        cbWrite(bus, proc.outCh, proc.outValue, ch.chainOwner);
    }
}


/**
 * @brief Update all channels — iterates the array and calls proc_chain_step().
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       Shared ComBus — forwarded to each proc_chain_step().
 */
void proc_chain_update(CbChain* channels, uint8_t count, ComBus& bus)
{
    for (uint8_t p = 0; p < count; ++p) {
        proc_chain_step(channels[p], bus);
    }
}

// EOF proc_chain.cpp
