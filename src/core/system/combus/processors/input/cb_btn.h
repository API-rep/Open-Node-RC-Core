/*!****************************************************************************
 * @file  cb_btn.h
 * @brief CbProc button functions — push, increment, decrement, toggle.
 *
 * @details Four independent button processors for read-modify-write chains
 *   on analog or digital ComBus channels.  Each proc reads the button input,
 *   evaluates a trigger condition, and on fire applies an action to the value.
 *
 *   Trigger modes (cfg.trigger + cfg.holdMs):
 *     - ON_PRESS,   holdMs==0 : fires immediately on press-down (rising edge).
 *     - ON_PRESS,   holdMs>0  : fires once while held >= holdMs (still pressed).
 *     - ON_RELEASE, holdMs==0 : fires on any button release (falling edge).
 *     - ON_RELEASE, holdMs>0  : fires on release only if held >= holdMs.
 *
 *   Repeat guard (ON_PRESS trigger): fired flag is set on fire and cleared on
 *   release — each new press fires at most once.  ON_RELEASE fires at most once
 *   per falling edge by definition.
 *
 *   Action applied when the trigger fires:
 *     - push:    val := cfg.bound
 *     - inc:     val := min(val + 1, cfg.bound)
 *     - dec:     val := max(val - 1, cfg.bound)
 *     - toggle:  val := (val > 0) ? 0 : cfg.bound
 *
 *   Placement: Multiple button procs chain on the same output channel
 *     (e.g., SUBGEAR_BUS or KEY_ACTIVE) — each reads the current value,
 *     modifies it, and passes it to the next proc.
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
 * @brief Trigger mode — determines when the button action fires.
 *
 * @details
 *   - ON_PRESS   : fires on (or while) pressed. Once per press (repeat-guarded by fired flag).
 *   - ON_RELEASE : fires on button release. Gated by holdMs if holdMs > 0.
 */
enum class CbBtnTrigger : uint8_t {
    ON_PRESS   = 0,  ///< Default — fires on rising edge (holdMs==0) or while held >= holdMs.
    ON_RELEASE = 1,  ///< Fires on falling edge, only if held >= holdMs (or any release if holdMs==0).
};

/**
 * @brief Configuration for all button processors.
 *
 * @details
 *   - `bound`   : push -> target value | inc -> upper limit | dec -> lower limit | toggle -> "on" value.
 *   - `holdMs`  : 0 = fire immediately on edge; >0 = hold duration threshold (ms).
 *   - `trigger` : ON_PRESS or ON_RELEASE (zero-init = ON_PRESS).
 */
struct CbBtnCfg {
    uint16_t      bound;    ///< Target or boundary value — semantics depend on processor type.
    uint16_t      holdMs;   ///< Hold duration threshold (ms). 0 = no hold required.
    CbBtnTrigger  trigger;  ///< Trigger mode (zero-init = ON_PRESS).
};

/**
 * @brief Mutable runtime state for button processors.
 *
 * @details Assigned to `CbProc::state` (as `void*`); cast back inside processor.
 *   Zero-initialise (`{}`) — all fields start at 0/false, which is the correct initial state.
 */
struct CbBtnState {
    bool      prevPressed;  ///< Previous frame button state — edge detection.
    uint32_t  pressMs;      ///< Rising-edge timestamp — hold duration measurement.
    bool      fired;        ///< Repeat guard — set on fire, cleared on release (ON_PRESS trigger only).
};


// =============================================================================
// 2. PUBLIC API — 4 FUNCTIONS
// =============================================================================

/**
 * @brief Button processor — PUSH (force value to cfg.bound).
 *
 * @details Matches `CbProcFn` signature.  When the configured gesture fires,
 *   sets `value := cfg.bound` and sets `claimed = true`.
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
 * @details Matches `CbProcFn` signature.  When the configured gesture fires,
 *   increments value up to cfg.bound.
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
 * @details Matches `CbProcFn` signature.  When the configured gesture fires,
 *   decrements value down to cfg.bound.
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
 * @details Matches `CbProcFn` signature.  When the configured gesture fires,
 *   toggles between 0 and cfg.bound.
 *
 * @param proc       CbProc descriptor — `inCh[0]` read as button state,
 *                   `cfg` cast to `const CbBtnCfg*`, `state` cast to `CbBtnState*`.
 * @param value      [in/out] Current channel value.
 * @param claimed    [in/out] Set to true if button fired.
 * @param chainOwner Owner info (unused).
 */
void cb_btn_toggle_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);


// EOF cb_btn.h
