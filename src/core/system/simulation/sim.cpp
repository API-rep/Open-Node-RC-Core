/******************************************************************************
 * @file  sim.cpp
 * @brief CbChain dispatcher — init and update implementation.
 *****************************************************************************/

#include "sim.h"

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
void sim_init(CbChain* /*channels*/, uint8_t /*count*/)
{
    // Stages self-init on first update call (zero-state detection).
}


/**
 * @brief Process a single CbChain — pre-read, dispatch processors, post-write.
 *
 * @details Sequence:
 *   1. Pre-read  : runner reads `ch.optInCh` → seeds `value`.
 *   2. Proc loop : for each proc (stops on `claimed = true`):
 *        a. Injects secondary inputs: `proc.secInValue[i]` ← bus[proc.secInCh[i]].
 *        b. Calls `proc.fn(&proc, value, claimed, ch.chainOwner)`.
 *        c. Commits secondary output: bus[proc.optSecOutCh] ← proc.secOutValue.
 *   3. Post-write: runner writes `value` → `ch.optOutCh`.
 *      Always executes — `claimed` only aborts the proc chain, not the write.
 *
 * @param ch   Channel descriptor (procs, chainOwner, optInCh, optOutCh).
 * @param bus  Shared ComBus for this cycle.
 */
void sim_chain_update(CbChain& ch, ComBus& bus)
{
    // --- 1. Pre-read primary input -------------------------------------------
    uint16_t value   = cbRead(bus, ch.optInCh, /*isDrivedGuard=*/true);
    bool     claimed = false;

    // --- 2. Process chain ----------------------------------------------------
    for (uint8_t p = 0; p < ch.procCount && !claimed; ++p) {
        CbProc& proc = ch.procs[p];
        if (proc.fn == nullptr) continue;

        //  a. Inject secondary inputs (no isDrived guard — internal channels).
        for (uint8_t i = 0u; i < 3u; ++i) {
            proc.secInValue[i] = cbRead(bus, proc.secInCh[i], /*isDrivedGuard=*/false);
        }

        //  b. Call proc fn (no bus access inside fn).
        proc.fn(&proc, value, claimed, ch.chainOwner);

        //  c. Commit secondary output.
        cbWrite(bus, proc.optSecOutCh, proc.secOutValue, ch.chainOwner);
    }

    // --- 3. Post-write primary output (always) -------------------------------
    cbWrite(bus, ch.optOutCh, value, ch.chainOwner);
}


/**
 * @brief Update all SimChannels — iterates the array and calls sim_chain_update().
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       Shared ComBus — forwarded to each sim_chain_update().
 */
void sim_update(CbChain* channels, uint8_t count, ComBus& bus)
{
    for (uint8_t p = 0; p < count; ++p) {
        sim_chain_update(channels[p], bus);
    }
}

// EOF sim.cpp
