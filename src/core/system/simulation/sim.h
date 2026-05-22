/******************************************************************************
 * @file  sim.h
 * @brief SimChannel dispatcher — init and update.
 *
 * @details Entry point for the simulation layer.  `sim_update()` iterates an
 *   array of SimChannel descriptors and dispatches each stage's SimProcFn
 *   in sequence.  Each channel carries its own `ComBus*` pointer (set once
 *   in the config array).  Stage functions must not call combus_set_analog()
 *   directly — the channel dispatcher owns the write.
 *
 *   Usage:
 *   @code
 *     // In machine config (envCfg.h):
 *     .simChannel      = kMyChannels,
 *     .simChannelCount = MY_CHANNEL_COUNT
 *
 *     // In main loop (RUNNING state):
 *     sim_update(machine.simChannel, machine.simChannelCount, comBus);
 *   @endcode
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INCLUDES
// =============================================================================

#include <struct/simulation_struct.h>   // SimChannel, SimProc, SimProcFn
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Placeholder lifecycle init for the SimChannel array.
 *
 * @details Stages self-init on first update call (zero-state detection inside
 *   each SimProcFn).  This function is kept as a symmetric counterpart to
 *   dc_dev_init() / srv_dev_init() and as a future explicit pre-flight hook.
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 */
void sim_init(SimChannel* channels, uint8_t count);

/**
 * @brief Update all channels — dispatch stages in order, write output channels.
 *
 * @details For each channel:
 *   1. Seeds `value` from `ch.bus->analogBus[outCh].value` (smooth hold from last tick).
 *   2. Iterates stages: calls `stage.fn()` when not nullptr.
 *      Stops iterating when a stage sets `claimed = true`.
 *   3. Writes the final `value` to `outCh` via `combus_set_analog(*ch.bus, ...)`.
 *
 *   Called once per loop cycle in the RUNNING runlevel block, after input_update().
 *   Each channel carries its own `ComBus*` (set at config time in the array).
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 */
/**
 * @brief Process a single SimChannel — seed, dispatch processors, write output.
 *
 * @details Called by sim_update() for each channel.  May also be called
 *   directly when only one channel needs to be refreshed out-of-order.
 *
 * @param ch   Channel to process.
 * @param bus  Shared ComBus for this channel.
 */
void sim_channel_update(SimChannel& ch, ComBus& bus);

/**
 * @brief Update all channels — dispatch processors in order, write output channels.
 *
 * @details For each channel:
 *   1. Seeds `value` from `bus.analogBus[inCh].value` (live input captured at cycle start).
 *   2. Iterates processors: calls `proc.fn()` when not nullptr.
 *      Stops iterating when a processor sets `claimed = true`.
 *   3. Writes the final `value` to `outCh` via `combus_set_analog(bus, ...)`.
 *
 *   Called once per loop cycle in the RUNNING runlevel block, after input_update().
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       Shared ComBus — passed to each sim_channel_update().
 */
void sim_update(SimChannel* channels, uint8_t count, ComBus& bus);


// EOF sim.h
