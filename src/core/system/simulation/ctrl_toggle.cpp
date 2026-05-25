/*!****************************************************************************
 * @file  ctrl_toggle.cpp
 * @brief Toggle CtrlProcFn — pure rising-edge toggle implementation.
 *****************************************************************************/

#include "ctrl_toggle.h"

#include <struct/ctrl_struct.h>    // CtrlProc, CtrlToggleState


// =============================================================================
// 1. PROC IMPLEMENTATION
// =============================================================================

void ctrl_toggle_fn(CtrlProc* proc, uint16_t& value,
                    bool& /*claimed*/, ChanOwner /*owner*/)
{
    auto* state = static_cast<CtrlToggleState*>(proc->state);

    const bool rising = (value != 0u) && (state->prevBtn == 0u);
    state->prevBtn = value;

    if (rising) state->active = (state->active != 0u) ? 0u : 1u;

    //  Forward the active state to the next proc (or to the runner post-write).
    value = state->active;
}

// EOF ctrl_toggle.cpp
