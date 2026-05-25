/*!****************************************************************************
 * @file  ctrl.h
 * @brief CtrlChannel dispatcher — runner for the digital ctrl pipeline.
 *
 * @details Entry point for the ctrl layer.  `ctrl_update()` iterates an array
 *   of CtrlChannel descriptors and dispatches each proc's CtrlProcFn in
 *   sequence.
 *
 *   Runner contract:
 *   - Reads `inCh` from the bus.  If `!isDrived`, the entire channel is skipped
 *     (all procs and the outCh write are no-ops this cycle).
 *   - Passes `value = bus.digitalBus[inCh].value` to proc[0].
 *   - After all procs, writes `outCh` via `combus_set_digital()` unless any
 *     proc set `claimed = true` (direct-write shortcut).
 *
 *   Usage:
 *   @code
 *     // In machine config (envCfg.h):
 *     .ctrlChannel      = kMyCtrlChannels,
 *     .ctrlChannelCount = MY_CTRL_CHANNEL_COUNT
 *
 *     // In main loop (RUNNING state, before sim_update):
 *     ctrl_update(machine.ctrlChannel, machine.ctrlChannelCount, comBus, owner);
 *   @endcode
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INCLUDES
// =============================================================================

#include <struct/ctrl_struct.h>    // CtrlChannel, CtrlProc, CtrlProcFn
#include <struct/combus_struct.h>  // ComBus, ChanOwner


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Update all ctrl channels — dispatch procs in order, write output channels.
 *
 * @param channels  Channel array (may be nullptr when count == 0).
 * @param count     Number of channels.
 * @param bus       ComBus — read by all procs; written by the runner.
 * @param owner     ChanOwner used for the output write after all procs.
 */
void ctrl_update(CtrlChannel* channels, uint8_t count, ComBus& bus, ChanOwner owner);

// EOF ctrl.h
