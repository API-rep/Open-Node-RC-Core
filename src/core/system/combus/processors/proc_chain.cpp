/******************************************************************************
 * @file  proc_chain.cpp
 * @brief CbChain dispatcher — init and update implementation.
 *****************************************************************************/

#include "proc_chain.h"

#include <core/system/combus/combus_defs.h>              // ComBus
#include <core/system/combus/combus_access.h>  // combus_set_analog, combus_set_digital


// =============================================================================
// 0. INTERNAL HELPERS
// =============================================================================

using ChanOpt = std::optional<std::variant<AnalogComBusID, DigitalComBusID>>;

/** @brief Read a channel variant from the bus into a uint16_t. */
static uint16_t cbRead(const ComBus& bus, const ChanOpt& ch)
{
    if (!ch.has_value()) return 0u;
    if (std::holds_alternative<AnalogComBusID>(*ch)) {
        return bus.analogBus[static_cast<uint8_t>(std::get<AnalogComBusID>(*ch))].value;
    }
    const uint8_t idx = static_cast<uint8_t>(std::get<DigitalComBusID>(*ch));
    return bus.digitalBus[idx].value ? 1u : 0u;
}

/** @brief Write a uint16_t value to a channel variant on the bus.
 *
 *  @note  Proc-chain outputs are internal computed values, not physical inputs.
 *         Chantier 12.6: the legacy `bus.isDrived` flag is gone — proc-chain
 *         outputs no longer touch link-health state.  Each input backend
 *         (PS4_BT, UART, ...) writes its own *_LINK_LOST contributor channel,
 *         and the aggregator chain (`remote_link_fallback_chain.cpp`) handles
 *         the OR-aggregation into `REMOTE_LINK_LOST`.
 */
static void cbWrite(ComBus& bus, const ChanOpt& ch, uint16_t value)
{
    if (!ch.has_value()) return;
    if (std::holds_alternative<AnalogComBusID>(*ch)) {
        combus_set_analog(bus, std::get<AnalogComBusID>(*ch), value);
    } else {
        combus_set_digital(bus, std::get<DigitalComBusID>(*ch), value != 0u);
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
 *   1. Seed value from chain.inCh (0 when inCh = nullopt).
 *   2. Proc loop — all procs, in order:
 *        a. Inject secondary input: `proc.inValue` ← bus[proc.inCh].
 *        b. Skip when `claimed = true`.
 *        c. Call `proc.fn(&proc, value, claimed)`.
 *        d. Commit proc side-output: bus[proc.outCh] ← proc.outValue.
 *   3. Commit final value to chain.outCh.
 *
 * @param ch   Channel descriptor (procs).
 * @param bus  Shared ComBus for this cycle.
 */
void proc_chain_step(CbChain& ch, ComBus& bus)
{
    // --- 1. Seed pipeline from chain.inCh ------------------------------------
    uint16_t value   = cbRead(bus, ch.inCh);
    bool     claimed = false;

    // --- 2. Process chain ----------------------------------------------------
    for (uint8_t p = 0; p < ch.procCount; ++p) {
        CbProc& proc = ch.procs[p];
        if (proc.fn == nullptr) continue;

        //  a. Inject secondary input from proc.inCh.
        proc.inValue = cbRead(bus, proc.inCh);

        //  b. Skip when claimed.
        if (claimed) continue;

        //  c. Call proc fn (no bus access inside fn).
        proc.fn(&proc, value, claimed);

        //  d. Commit proc side-output.
        cbWrite(bus, proc.outCh, proc.outValue);
    }

    // --- 3. Commit final pipeline value to chain.outCh -----------------------
    cbWrite(bus, ch.outCh, value);
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
