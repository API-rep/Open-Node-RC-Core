/*!****************************************************************************
 * @file  cb_btn.h
 * @brief CbProc button functions — push, increment, decrement, toggle.
 *
 * @details Four independent button processors for read-modify-write chains
 *   on analog ComBus channels (e.g., SUBGEAR_BUS).  Each proc detects rising
 *   edges on a digital input and modifies the channel value accordingly.
 *
 *   Rising-edge behavior (default):
 *     - push:    val := cfg.bound
 *     - inc:     val := min(val + 1, cfg.bound)
 *     - dec:     val := max(val - 1, cfg.bound)
 *     - toggle:  val := (val > 0) ? 0 : cfg.bound
 *
 *   Long-press mode (holdMs > 0):
 *     Behavior is triggered only when the button is held for at least
 *     cfg.holdMs milliseconds. Prevents accidental triggers on brief bounces.
 *     holdFired prevents repeated execution while the button remains pressed.
 *
 *   Placement: Multiple button procs can chain on the same output channel
 *     (e.g., SUBGEAR_BUS) — each reads the current value, modifies it,
 *     and passes it to the next proc.  Final runner commits to the channel.
 *
 *   Typical declaration (in combus_input_config.cpp):
 *   @code
 *     { .name  = "subgear_toggle",
 *       .inCh  = { DigitalComBusID::SUBGEAR_SET_BTN },
 *       .outCh = AnalogComBusID::SUBGEAR_BUS,
 *       .fn    = cb_btn_toggle_fn,
 *       .cfg   = &kSubGearToggleCfg,
 *       .state = &gSubGearToggleState },
 *   @endcode
 *******************************************************************************
 */
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc


// =============================================================================
// 1. SHARED CONFIG & STATE
// =============================================================================

/**
 * @brief Configuration for all button processors.
 *
 * @details
 *   - `bound` : push → target value | inc → upper limit | dec → lower limit | toggle → "on" value
 *   - `debounceMs` : minimum ms between transitions to ignore noise (0 = disabled)
 *   - `holdMs` : long-press threshold (0 = rising-edge only ; >0 = press-and-hold required)
 */
struct CbBtnCfg {
    uint16_t  bound;        ///< Target or boundary value — semantics depend on processor type.
    uint16_t  debounceMs;   ///< Anti-bounce filter (ms).
    uint16_t  holdMs;       ///< Long-press threshold (ms) ; 0 = rising-edge mode.
};

/**
 * @brief Mutable runtime state for button processors.
 *
 * @details Assigned to `CbProc::state` (as `void*`); cast back inside processor.
 */
struct CbBtnState {
    bool      prevPressed;  ///< Previous button state — rising-edge detection.
    uint32_t  lastPressMs;  ///< Timestamp of last press — debounce + hold guard.
    bool      holdFired;    ///< Long-press fired flag — prevents repeat while held.
};


// =============================================================================
// 2. PUBLIC API — 4 FUNCTIONS
// =============================================================================

/**
 * @brief Button processor — PUSH (force value to cfg.bound).
 *
 * @details Matches `CbProcFn` signature.  On button rising edge (or after
 *   holdMs if configured), sets `value := cfg.bound` and sets `claimed = true`.
 *
 * @param proc       CbProc descriptor — `inCh[0]` read as button state,
 *                   `cfg` cast to `const CbBtnCfg*`, `state` cast to `CbBtnState*`.
 * @param value      [in/out] Current channel value.
 * @param claimed    [in/out] Set to true if button fired.
 * @param chainOwner Owner info (unused).
 */
void cb_btn_push_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Button processor — INCREMENT (value := min(value + 1, cfg.bound)).
 *
 * @details Matches `CbProcFn` signature.  On button rising edge (or after
 *   holdMs if configured), increments value up to cfg.bound.
 *
 * @param proc       CbProc descriptor — `inCh[0]` read as button state,
 *                   `cfg` cast to `const CbBtnCfg*`, `state` cast to `CbBtnState*`.
 * @param value      [in/out] Current channel value.
 * @param claimed    [in/out] Set to true if button fired.
 * @param chainOwner Owner info (unused).
 */
void cb_btn_inc_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Button processor — DECREMENT (value := max(value - 1, cfg.bound)).
 *
 * @details Matches `CbProcFn` signature.  On button rising edge (or after
 *   holdMs if configured), decrements value down to cfg.bound.
 *
 * @param proc       CbProc descriptor — `inCh[0]` read as button state,
 *                   `cfg` cast to `const CbBtnCfg*`, `state` cast to `CbBtnState*`.
 * @param value      [in/out] Current channel value.
 * @param claimed    [in/out] Set to true if button fired.
 * @param chainOwner Owner info (unused).
 */
void cb_btn_dec_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Button processor — TOGGLE (value := (value > 0) ? 0 : cfg.bound).
 *
 * @details Matches `CbProcFn` signature.  On button rising edge (or after
 *   holdMs if configured), toggles between 0 and cfg.bound.
 *
 * @param proc       CbProc descriptor — `inCh[0]` read as button state,
 *                   `cfg` cast to `const CbBtnCfg*`, `state` cast to `CbBtnState*`.
 * @param value      [in/out] Current channel value.
 * @param claimed    [in/out] Set to true if button fired.
 * @param chainOwner Owner info (unused).
 */
void cb_btn_toggle_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);


// EOF cb_btn.h
