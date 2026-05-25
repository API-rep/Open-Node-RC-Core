/*!****************************************************************************
 * @file  ctrl_toggle.cpp
 * @brief Toggle CtrlProcFn — pure rising-edge toggle implementation.
 *****************************************************************************/

#include "ctrl_toggle.h"

#include <struct/ctrl_struct.h>    // CtrlProc, CtrlToggleState
#include <struct/combus_struct.h>  // ComBus (signature only)


// =============================================================================
// 1. PROC IMPLEMENTATION
// =============================================================================

void ctrl_toggle_fn(CtrlProc* proc, bool& value, ComBus& /*bus*/,
                    bool& /*claimed*/, ChanOwner /*owner*/)
{
    auto* state = static_cast<CtrlToggleState*>(proc->state);

    const bool rising = value && !state->prevBtn;
    state->prevBtn = value;

    if (rising) state->active = !state->active;

    //  Forward the active state to the next proc (or to ctrl_write_fn).
    value = state->active;
}

// EOF ctrl_toggle.cpp
