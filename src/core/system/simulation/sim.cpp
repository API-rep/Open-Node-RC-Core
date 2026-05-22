/******************************************************************************
 * @file  sim.cpp
 * @brief SimChannel dispatcher — init and update implementation.
 *****************************************************************************/

#include "sim.h"
#include "core/system/combus/combus_access.h"   // combus_set_analog


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
 * @brief Process a single SimChannel — seed, dispatch processors, write output.
 *
 * @details Sequence:
 *   1. Seeds `value` from `bus.analogBus[ch.inCh].value` — live capture at cycle start.
 *   2. Iterates `ch.simProc[]`; skips entries where `fn == nullptr` (passthrough).
 *      Stops early when a processor sets `claimed = true`.
 *   3. Writes `value` to `ch.outCh` via `combus_set_analog()` — always,
 *      regardless of `claimed`.
 *
 * @param ch   Channel descriptor (inCh, outCh, simProc array).
 * @param bus  Shared ComBus for this cycle.
 */
void sim_channel_update(SimChannel& ch, ComBus& bus)
{
    // --- 1. Seed value from SimChannel input — live capture at cycle start -----
    uint16_t value   = bus.analogBus[static_cast<uint8_t>(ch.inCh)].value;
    bool     claimed = false;

    // --- 2. Run processors in order, stop when claimed -----------------------
    for (uint8_t p = 0; p < ch.simProcCount && !claimed; ++p) {
        SimProc& proc = ch.simProc[p];
        if (proc.fn != nullptr) {
            proc.fn(&proc, value, bus, claimed);
        }
    }

    // --- 3. Write output — always, regardless of claimed ---------------------
    combus_set_analog(bus, ch.outCh, value, ch.chanOwner);
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

// EOF sim.cpp
