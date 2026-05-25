/*!****************************************************************************
 * @file  ctrl_toggle.cpp
 * @brief Toggle CtrlProcFn — rising-edge toggle with optional speed-gate on engage.
 *****************************************************************************/

#include "ctrl_toggle.h"

#include <struct/ctrl_struct.h>    // CtrlProc, CtrlToggleCfg, CtrlToggleState
#include <struct/combus_struct.h>  // ComBus


// =============================================================================
// 1. PROC IMPLEMENTATION
// =============================================================================

void ctrl_toggle_fn(CtrlProc* proc, bool& value, ComBus& bus,
                    bool& /*claimed*/, ChanOwner /*owner*/)
{
    const auto* cfg   = static_cast<const CtrlToggleCfg*>(proc->cfg);
    auto*       state = static_cast<CtrlToggleState*>(proc->state);

    const bool rising = value && !state->prevBtn;
    state->prevBtn = value;

    if (rising) {
        if (!state->active) {
            //  Engage: apply speed guard when configured.
            bool canEngage = true;
            if (cfg && cfg->speedCh.has_value() && cfg->maxEngageSpd > 0u) {
                const uint16_t spd =
                    bus.analogBus[static_cast<uint8_t>(cfg->speedCh.value())].value;
                canEngage = (spd <= cfg->maxEngageSpd);
            }
            if (canEngage) state->active = true;
        } else {
            //  Disengage: always immediate.
            state->active = false;
        }
    }

    //  Forward the active state to the runner (it will write outCh).
    value = state->active;
}

// EOF ctrl_toggle.cpp
