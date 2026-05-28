/******************************************************************************
 * @file  cb_cruise_struct.h
 * @brief Throttle hold (cruise-control) CbProc — config and state structures.
 *
 * @details Used by `cb_cruise_fn` in `src/core/system/combus/processors/motion/`.
 *   `CbCruiseCfg::active` can be toggled at runtime from any chain or module
 *   that holds a pointer to the RAM-resident config instance.
 *   The proc is independent of the gear module.
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. PROC CONFIG
// =============================================================================

/**
 * @brief Configuration for `cb_cruise_fn` — throttle hold.
 *
 * @details Must be RAM-resident (not constexpr) so `active` can be toggled
 *   at runtime from any module without coupling to the chain internals.
 *
 *   `active = false` : passthrough — `value` unchanged.
 *   `active = true`  : HOLD+NUDGE  — `value` is memorised on rising edge;
 *     stick deflection nudges the set-point (winter 2026 — not yet implemented).
 */
struct CbCruiseCfg {
    bool active; ///< false = passthrough; true = hold+nudge (winter 2026).
};


// =============================================================================
// 2. PROC STATE
// =============================================================================

/**
 * @brief Runtime state for `cb_cruise_fn`.
 */
struct CbCruiseState {
    uint16_t heldValue; ///< Currently held throttle value — set on rising `active` edge.
    bool     wasActive; ///< Previous `active` state — rising-edge detection for hold init.
};

// EOF cb_cruise_struct.h
