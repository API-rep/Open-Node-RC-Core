/*!****************************************************************************
 * @file  ctrl_toggle.h
 * @brief Toggle CtrlProcFn — pure rising-edge toggle (ON/OFF flip).
 *
 * @details Flips the `active` state on each rising edge of the button input.
 *   No speed guard, no bus access — pure button-state logic.
 *   To add a speed guard, chain `ctrl_speed_gate_fn` before this proc.
 *
 *   @code
 *     static CtrlToggleState gState {};
 *     { "toggle", ctrl_toggle_fn, nullptr, &gState },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/ctrl_struct.h>  // CtrlProc, CtrlToggleState, CtrlProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Toggle CtrlProcFn — flips active ON/OFF on each rising button edge.
 *
 * @param proc    Proc descriptor: `cfg → nullptr`, `state → CtrlToggleState*`.
 * @param value   Button state (in) — after the call, carries the resulting `active` flag.
 * @param bus     Unused.
 * @param claimed Unused.
 * @param owner   Unused.
 */
void ctrl_toggle_fn(CtrlProc* proc, uint16_t& value,
                    bool& claimed, ChanOwner owner);

// EOF ctrl_toggle.h
