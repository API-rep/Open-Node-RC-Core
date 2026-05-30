/******************************************************************************
 * @file  cb_btn_latch.h
 * @brief CbProc — latch button processor.
 *
 * @details Generic two-state latch driven by a single digital button.
 *
 *   Behavior (asymmetric by design):
 *     - Rising edge while latch = OFF  → latch = ON   (short press to activate)
 *     - Hold ≥ holdMs while latch = ON → latch = OFF  (long press to deactivate)
 *     - Short press while latch = ON   → no effect    (prevents accidental toggle)
 *
 *   The processor does NOT access ComBus directly.  It outputs a plain 0/1 to
 *   the pipeline which downstream procs (e.g. cb_runlevel_fn, cb_out_fn) act on.
 *
 *   Placement in a proc chain:
 *   @code
 *     cb_in_fn(BTN_CH)  →  cb_btn_latch_fn  →  cb_out_fn(LATCH_CH)  →  ...
 *   @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_proc_struct.h>  // CbProc


// =============================================================================
// 1. CONFIG & STATE
// =============================================================================

/**
 * @brief Static configuration for the latch button processor.
 */
struct CbBtnLatchCfg {
    uint16_t holdMs;   ///< Hold duration (ms) to clear the latch.  Must be > 0.
};

/**
 * @brief Mutable runtime state for the latch button processor.
 *
 * @details Assign to `CbProc::state` (as `void*`); cast back inside fn.
 *   Zero-init is valid initial state — `latch = false`, all timers disarmed.
 */
struct CbBtnLatchState {
    bool     prevPressed;   ///< Previous button state for rising-edge detection.
    bool     latch;         ///< Current latch value — persists across button releases.
    uint32_t holdStartMs;   ///< Timestamp when hold timer was armed (0 = disarmed).
    bool     holdFired;     ///< True after hold fires — prevents re-trigger while held.
};


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Latch button processor function.
 *
 * @details Matches `CbProcFn` signature.  Reads the button state from
 *   `proc->inValue` (injected by the runner from the configured `inCh`).
 *
 *   Transition rules:
 *     - Rising edge, latch=false  → latch=true  (arm: holdStartMs = 0)
 *     - Rising edge, latch=true   → arm hold timer  (holdStartMs = now)
 *     - Hold ≥ holdMs, latch=true → latch=false, holdFired=true
 *     - Release                   → holdStartMs=0, holdFired=false
 *
 *   Output: `value = latch ? 1u : 0u`.
 *
 * @param proc        CbProc descriptor.  `cfg` = CbBtnLatchCfg*,
 *                    `state` = CbBtnLatchState*, `inCh` = button channel.
 * @param value       Pipeline value written with current latch state.
 * @param claimed     Unused — latch always writes its output.
 * @param chainOwner  Chain identity forwarded from the runner.
 */
void cb_btn_latch_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);


// EOF cb_btn_latch.h
