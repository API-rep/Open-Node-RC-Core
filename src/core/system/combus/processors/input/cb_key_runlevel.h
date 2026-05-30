/******************************************************************************
 * @file  cb_key_runlevel.h
 * @brief CbProc — ignition key → RunLevel bridge.
 *
 * @details Processes the raw KEY_BTN digital channel and drives the RunLevel
 *   state machine directly:
 *   - Rising edge in IDLE or SLEEPING  → RunLevel::STARTING, KEY_ACTIVE = 1.
 *   - Sustained hold (≥ shutdownHoldMs) in RUNNING → RunLevel::IDLE, KEY_ACTIVE = 0.
 *
 *   Outputs a persistent KEY_ACTIVE flag (1 = active, 0 = inactive) so that
 *   cb_out_fn can commit it to the KEY_ACTIVE channel each cycle.
 *
 *   The ComBus pointer is stored in CbKeyRunlevelState::bus so that the
 *   proc can call combus_set_runlevel() without a ComBus parameter on CbProcFn.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_struct.h>       // ComBus, RunLevel
#include <struct/combus_proc_struct.h>  // CbProc, ChanOwner


// =============================================================================
// 1. CONFIG
// =============================================================================

/**
 * @brief Configuration for the ignition key runlevel processor.
 */
struct CbKeyRunlevelCfg {
    uint16_t shutdownHoldMs;  ///< Hold duration (ms) to trigger RUNNING → IDLE shutdown.
};


// =============================================================================
// 2. STATE
// =============================================================================

/**
 * @brief Mutable runtime state for the ignition key runlevel processor.
 *
 * @details `bus` must be set to the active ComBus pointer before the first
 *   proc_chain_update() call (typically at the end of machine_init()).
 */
struct CbKeyRunlevelState {
    ComBus*  bus;           ///< Target ComBus — set at init, never null during operation.
    bool     prevPressed;   ///< Previous KEY_BTN state — rising-edge detection.
    uint32_t holdStartMs;   ///< millis() when sustained hold began (0 = not held).
    bool     holdFired;     ///< True after the 3-second shutdown event fires — prevents repeat.
    bool     keyActive;     ///< Persistent KEY_ACTIVE value — 1 after start, 0 after shutdown.
};


// =============================================================================
// 3. PUBLIC API
// =============================================================================

/**
 * @brief Ignition key runlevel processor.
 *
 * @details Matches CbProcFn signature.
 *
 *   - Rising edge on KEY_BTN while IDLE or SLEEPING  → combus_set_runlevel(STARTING),
 *     sets value = 1.
 *   - Sustained hold ≥ cfg.shutdownHoldMs while RUNNING → combus_set_runlevel(IDLE),
 *     sets value = 0.
 *   - All other cycles: value = persistent keyActive flag (unchanged).
 *
 * @param proc       CbProc descriptor — cfg cast to CbKeyRunlevelCfg*, state cast to
 *                   CbKeyRunlevelState*.  proc->inValue = current KEY_BTN raw state.
 * @param value      [in/out] Set to the persistent KEY_ACTIVE flag each cycle.
 * @param claimed    Unused (not set — KEY_ACTIVE always updated every cycle).
 * @param chainOwner Forwarded to combus_set_runlevel() as the change owner.
 */
void cb_key_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);


// EOF cb_key_runlevel.h
