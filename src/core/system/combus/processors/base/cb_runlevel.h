/******************************************************************************
 * @file  cb_runlevel.h
 * @brief CbProc — ComBus RunLevel writer driven by a digital channel.
 *
 * @details Watches the pipeline input for 0↔1 transitions and updates the
 *   ComBus RunLevel accordingly:
 *     - Rising  edge (0→1) while idle/sleeping → sets `activeLevel`  (e.g. STARTING)
 *     - Falling edge (1→0) while starting/running → sets `defaultLevel` (e.g. TURNING_OFF)
 *
 *   The pipeline `value` is **not modified** — this processor is a side-effect
 *   only.  It is typically placed at the end of a chain, after the output proc
 *   has already committed the new state to the ComBus channel.
 *
 *   Placement in a proc chain:
 *   @code
 *     ... → cb_out_fn(KEY_ACTIVE) → cb_runlevel_fn    // KEY_ACTIVE already written
 *   @endcode
 *****************************************************************************/
#pragma once

#include <defs/machines_defs.h>         // RunLevel
#include <struct/combus_struct.h>        // ComBus
#include <struct/combus_proc_struct.h>   // CbProc


// =============================================================================
// 1. CONFIG & STATE
// =============================================================================

/**
 * @brief Static configuration for the RunLevel writer processor.
 */
struct CbRunlevelCfg {
    RunLevel activeLevel;    ///< RunLevel to set on rising edge  (e.g. STARTING).
    RunLevel defaultLevel;   ///< RunLevel to set on falling edge (e.g. TURNING_OFF).
};

/**
 * @brief Mutable runtime state for the RunLevel writer processor.
 *
 * @details Assign `bus` before the proc chain runs for the first time.
 *   Zero-init is valid for `prevValue`.
 */
struct CbRunlevelState {
    ComBus* bus;        ///< ComBus instance to write.  Must not be nullptr.
    bool    prevValue;  ///< Previous active state for edge detection.
};


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief RunLevel writer processor function.
 *
 * @details Matches `CbProcFn` signature.  Reads the channel state from
 *   `proc->inValue` (injected by the runner from the configured `inCh`).
 *
 *   Transition rules (RunLevel guards prevent redundant writes):
 *     - 0→1 AND runLevel ∈ {IDLE, SLEEPING}        → combus_set_runlevel(activeLevel)
 *     - 1→0 AND runLevel ∈ {STARTING, RUNNING}     → combus_set_runlevel(defaultLevel)
 *
 *   `value` is passed through unchanged.
 *
 * @param proc        CbProc descriptor.  `cfg` = CbRunlevelCfg*,
 *                    `state` = CbRunlevelState*, `inCh` = monitored channel.
 * @param value       Pipeline value — unchanged (pass-through).
 * @param claimed     Unused — this processor never claims the channel.
 * @param chainOwner  Forwarded to `combus_set_runlevel` as the caller identity.
 */
void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);


// EOF cb_runlevel.h
