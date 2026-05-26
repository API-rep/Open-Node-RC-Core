/******************************************************************************
 * @file  proc_chain.h
 * @brief CbChain dispatcher — init and update.
 *
 * @details Entry point for the CbChain processor pipeline.  `proc_chain_update()`
 *   iterates an array of CbChain descriptors and dispatches each stage's
 *   CbProcFn in sequence.
 *
 *   Runner contract:
 *   - Pre-reads `ch.optInCh` to seed `value`.
 *   - Iterates procs: injects `inValue[]`, calls fn, commits `outValue`.
 *   - Post-writes `ch.outCh` after the chain (regardless of `claimed`).
 *
 *   Usage:
 *   @code
 *     // In machine config (envCfg.h):
 *     .simChain      = kMyChannels,
 *     .simChainCount = MY_CHANNEL_COUNT
 *
 *     // In main loop (RUNNING state):
 *     proc_chain_update(machine.simChain, machine.simChainCount, comBus);
 *   @endcode
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INCLUDES
// =============================================================================

#include <struct/simulation_struct.h>   // CbChain, CbProc
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Placeholder lifecycle init for the CbChain array.
 *
 * @details Stages self-init on first update call (zero-state detection inside
 *   each CbProcFn).  Kept as a symmetric counterpart to dc_dev_init().
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 */
void proc_chain_init(CbChain* channels, uint8_t count);

/**
 * @brief Process a single CbChain.
 *
 * @details Pre-reads optInCh, dispatches procs with inValue injection and
 *   outValue commit, then post-writes outCh.  May be called directly when
 *   only one channel needs to be refreshed out-of-order.
 *
 * @param ch   Channel to process.
 * @param bus  Shared ComBus for this channel.
 */
void proc_chain_step(CbChain& ch, ComBus& bus);

/**
 * @brief Update all channels.
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       Shared ComBus.
 */
void proc_chain_update(CbChain* channels, uint8_t count, ComBus& bus);

// EOF proc_chain.h
