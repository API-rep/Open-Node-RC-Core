/******************************************************************************
 * @file  sim.cpp
 * @brief SimChannel dispatcher — init and update implementation.
 *****************************************************************************/

#include "sim.h"
#include "sim_io.h"                              // sim_read_fn, sim_write_fn (for helpers)


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Placeholder lifecycle init for the SimChannel array.
 *
 * @details Kept as a symmetric counterpart to dc_dev_init() / srv_dev_init().
 *   Each SimProcFn self-inits on first update call via zero-state detection
 *   (e.g. SimRampState::currentPos == 0 → snap to CbusNeutral).
 *   No explicit pre-flight work is needed at this stage.
 */
void sim_init(SimChannel* /*channels*/, uint8_t /*count*/)
{
    // Stages self-init on first update call (zero-state detection).
}


/**
 * @brief Process a single SimChannel — dispatch processors, owned by sim_read_fn / sim_write_fn.
 *
 * @details Sequence:
 *   1. Iterates `ch.simProc[]`; skips entries where `fn == nullptr` (passthrough).
 *      Stops early when a processor sets `claimed = true`.
 *   2. `sim_read_fn` (first proc) seeds `value` from the input channel.
 *   3. `sim_write_fn` (last proc) writes `value` to the output channel.
 *   4. `sim_bypass_fn` writes directly to its `outCh` and sets `claimed = true`,
 *      skipping all remaining procs including `sim_write_fn`.
 *
 *   `ch.chanOwner` is forwarded to every `SimProcFn` call so that
 *   `sim_write_fn` and `sim_bypass_fn` can identify the writer.
 *
 * @param ch   Channel descriptor (simProc array, chanOwner).
 * @param bus  Shared ComBus for this cycle.
 */
void sim_channel_update(SimChannel& ch, ComBus& bus)
{
    // --- Run processors in order, stop when claimed --------------------------
    uint16_t value   = 0u;
    bool     claimed = false;
    for (uint8_t p = 0; p < ch.simProcCount && !claimed; ++p) {
        SimProc& proc = ch.simProc[p];
        if (proc.fn != nullptr) {
            proc.fn(&proc, value, bus, claimed, ch.chanOwner);
        }
    }
    // No seed, no write here — sim_read_fn / sim_write_fn (or sim_bypass_fn) own these.
}


/**
 * @brief Update all SimChannels — iterates the array and calls sim_channel_update().
 *
 * @details Called once per loop cycle in the RUNNING runlevel, after input_update().
 *   A nullptr array with count == 0 is a valid no-op configuration.
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       Shared ComBus — forwarded to each sim_channel_update().
 */
void sim_update(SimChannel* channels, uint8_t count, ComBus& bus)
{
    for (uint8_t p = 0; p < count; ++p) {
        sim_channel_update(channels[p], bus);
    }
}


// =============================================================================
// 2. DASHBOARD HELPERS  (scan proc chain — debug use only)
// =============================================================================

/** @brief Returns the source channel of the first sim_read_fn proc. */
AnalogComBusID sim_channel_read_ch(const SimChannel& ch)
{
    for (uint8_t p = 0; p < ch.simProcCount; ++p) {
        const auto& opt = ch.simProc[p].optInCh;
        if (ch.simProc[p].fn == sim_read_fn && opt.has_value()
            && std::holds_alternative<AnalogComBusID>(*opt))
            return std::get<AnalogComBusID>(*opt);
    }
    return AnalogComBusID{};
}

/** @brief Returns the target channel of the last sim_write_fn proc. */
AnalogComBusID sim_channel_write_ch(const SimChannel& ch)
{
    for (uint8_t p = 0; p < ch.simProcCount; ++p) {
        const auto& opt = ch.simProc[p].optOutCh;
        if (ch.simProc[p].fn == sim_write_fn && opt.has_value()
            && std::holds_alternative<AnalogComBusID>(*opt))
            return std::get<AnalogComBusID>(*opt);
    }
    return AnalogComBusID{};
}

// EOF sim.cpp
