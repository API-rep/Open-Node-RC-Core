/*!****************************************************************************
 * @file  ctrl.h
 * @brief CbChain dispatcher for ctrl layer — digital proc pipeline runner.
 *
 * @details Entry point for the ctrl layer.  `ctrl_update()` iterates an array
 *   of CbChain descriptors and dispatches each proc's CbProcFn in sequence.
 *
 *   Runner contract (same as sim runner):
 *   - Pre-reads `ch.optInCh` (with isDrived guard) to seed `value`.
 *   - For each proc: injects `secInValue[]`, calls fn, commits `secOutValue`.
 *   - Post-writes `ch.optOutCh` after the chain (regardless of `claimed`).
 *   - `ChanOwner` comes from `ch.chainOwner` — no external owner parameter.
 *
 *   Usage:
 *   @code
 *     // In machine config (envCfg.h):
 *     .ctrlChain      = kMyCtrlChannels,
 *     .ctrlChainCount = MY_CTRL_CHANNEL_COUNT
 *
 *     // In main loop (RUNNING state, before sim_update):
 *     ctrl_update(machine.ctrlChain, machine.ctrlChainCount, comBus);
 *   @endcode
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INCLUDES
// =============================================================================

#include <struct/ctrl_struct.h>    // CbChain (= CbChain), CbProc (= CbProc)
#include <struct/combus_struct.h>  // ComBus


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Update all ctrl channels — dispatch procs in order, write output channels.
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       ComBus — read/written by the runner (not passed to fn).
 */
void ctrl_update(CbChain* channels, uint8_t count, ComBus& bus);

// EOF ctrl.h
