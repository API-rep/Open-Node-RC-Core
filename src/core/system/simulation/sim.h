/******************************************************************************
 * @file  sim.h
 * @brief SimChannel dispatcher — init and update.
 *
 * @details Entry point for the simulation layer.  `sim_update()` iterates an
 *   array of SimChannel descriptors and dispatches each stage's CbProcFn
 *   in sequence.
 *
 *   Runner contract:
 *   - Pre-reads `ch.optInCh` to seed `value`.
 *   - Iterates procs: injects `secInValue[]`, calls fn, commits `secOutValue`.
 *   - Post-writes `ch.optOutCh` after the chain (regardless of `claimed`).
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

#include <struct/simulation_struct.h>   // SimChannel (= CbChannel), SimProc (= CbProc)
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Placeholder lifecycle init for the SimChannel array.
 *
 * @details Stages self-init on first update call (zero-state detection inside
 *   each CbProcFn).  Kept as a symmetric counterpart to dc_dev_init().
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 */
void sim_init(SimChannel* channels, uint8_t count);

/**
 * @brief Process a single SimChannel.
 *
 * @details Pre-reads optInCh, dispatches procs with secIn injection and
 *   secOut commit, then post-writes optOutCh.  May be called directly when
 *   only one channel needs to be refreshed out-of-order.
 *
 * @param ch   Channel to process.
 * @param bus  Shared ComBus for this channel.
 */
void sim_channel_update(SimChannel& ch, ComBus& bus);

/**
 * @brief Update all channels.
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       Shared ComBus.
 */
void sim_update(SimChannel* channels, uint8_t count, ComBus& bus);

// EOF sim.h
