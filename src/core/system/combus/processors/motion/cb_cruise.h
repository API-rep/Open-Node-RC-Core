/******************************************************************************
 * @file  cb_cruise.h
 * @brief CbProc function — throttle hold (cruise-control, HOLD+NUDGE).
 *
 * @details `cb_cruise_fn()` is placed in any chain where cruise-control hold
 *   is needed (typically the THROTTLE chain).  It is completely independent
 *   of the gear module.
 *
 *   `CbCruiseCfg::active = false` : passthrough — `value` unchanged.
 *   `CbCruiseCfg::active = true`  : HOLD+NUDGE — `value` is memorised in
 *     `CbCruiseState::heldValue` on rising edge; subsequent calls apply a
 *     proportional delta from stick deflection.  (winter 2026 — stub only)
 *
 *   Any proc or module that holds a pointer to the RAM-resident `CbCruiseCfg`
 *   instance can toggle cruise-control without coupling to this proc's chain.
 *
 *   cfg   = CbCruiseCfg*   (must be RAM-resident — mutable at runtime).
 *   state = CbCruiseState*.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                         // CbProc, ChanOwner
#include <struct/combus/processors/motion/cb_cruise_struct.h>  // CbCruiseCfg, CbCruiseState


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Throttle hold proc — assigned to `CbProc::fn`.
 *
 * @param proc       CbProc descriptor. `cfg` = CbCruiseCfg* (RAM); `state` = CbCruiseState*.
 * @param value      In/out throttle value in channel domain.
 * @param claimed    Not modified.
 * @param chainOwner Not used.
 */
void cb_cruise_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_cruise.h
