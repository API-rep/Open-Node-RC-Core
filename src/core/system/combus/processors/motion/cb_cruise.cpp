/******************************************************************************
 * @file  cb_cruise.cpp
 * @brief CbProc function — throttle hold (cruise-control, HOLD+NUDGE).
 *****************************************************************************/

#include "cb_cruise.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Throttle hold — see cb_cruise.h for full contract. */
void cb_cruise_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const CbCruiseCfg* cfg   = static_cast<const CbCruiseCfg*>(proc->cfg);
    CbCruiseState*     state = static_cast<CbCruiseState*>(proc->state);

    if (!cfg->active) {
        // Passthrough. Reset edge-detection so the next activation re-latches.
        state->wasActive = false;
        return;
    }

    // Rising edge: latch current value as the cruise set-point.
    if (!state->wasActive) {
        state->heldValue = value;
        state->wasActive = true;
    }

    // TODO winter 2026: HOLD+NUDGE — apply stick delta to state->heldValue and clamp.
    //   delta = (value − CbusNeutral) × nudgeRate;
    //   state->heldValue = clamp(state->heldValue + delta, 0, CbusMaxVal);
    value = state->heldValue;
}

// EOF cb_cruise.cpp
