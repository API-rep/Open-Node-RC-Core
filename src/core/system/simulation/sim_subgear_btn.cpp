/******************************************************************************
 * @file  sim_subgear_btn.cpp
 * @brief SimProc function — sub-gear mode button handler (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Reads three secondary digital inputs (SET, UP, DOWN) via secInValue[];
 *   emits sub-gear index to secOutValue (runner commits to SUBGEAR_BUS).
 *****************************************************************************/

#include "sim_subgear_btn.h"

#include <Arduino.h>  // millis()


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Sub-gear button handler — see sim_subgear_btn.h for full contract. */
void sim_subgear_btn_fn(SimProc* proc, uint16_t& /*value*/, bool& /*claimed*/, ChanOwner /*chanOwner*/)
{
    const SimSubGearBtnCfg* cfg   = static_cast<const SimSubGearBtnCfg*>(proc->cfg);
    SimSubGearBtnState*     state = static_cast<SimSubGearBtnState*>(proc->state);

    // --- 0. Self-init (subGear == 0 on zero-init state) ----------------------
    // Nothing to do — zero is the "off" state; valid as-is.

    // --- 1. Read secondary inputs --------------------------------------------
    //  secInCh[0] = SUBGEAR_SET_BTN (digital)
    //  secInCh[1] = SUBGEAR_UP_BTN  (digital)
    //  secInCh[2] = SUBGEAR_DOWN_BTN (digital)
    const bool setNow = (proc->secInValue[0] != 0u);
    const bool upNow  = (proc->secInValue[1] != 0u);
    const bool dnNow  = (proc->secInValue[2] != 0u);

    // --- 2. Debounce guard (optional) ----------------------------------------
    if (cfg->debounceMs > 0u) {
        const uint32_t now = millis();
        if ((setNow && !state->prevSet) || (upNow && !state->prevUp) || (dnNow && !state->prevDn)) {
            if (now - state->lastMs < cfg->debounceMs) {
                // Still within debounce window — update prev states, skip action.
                state->prevSet = setNow;
                state->prevUp  = upNow;
                state->prevDn  = dnNow;
                proc->secOutValue = static_cast<uint16_t>(state->subGear);
                return;
            }
            state->lastMs = now;
        }
    }

    // --- 3. SET: toggle sub-gear mode on rising edge -------------------------
    if (setNow && !state->prevSet) {
        state->subGear = (state->subGear == 0) ? 1 : 0;
    }

    // --- 4. UP: step up on rising edge (mode must be active) -----------------
    if (state->subGear > 0 && upNow && !state->prevUp) {
        if (state->subGear < static_cast<int8_t>(cfg->subGearCount))
            state->subGear++;
    }

    // --- 5. DOWN: step down on rising edge (mode must be active) -------------
    if (state->subGear > 0 && dnNow && !state->prevDn) {
        if (state->subGear > 1)
            state->subGear--;
    }

    // --- 6. Update prev states -----------------------------------------------
    state->prevSet = setNow;
    state->prevUp  = upNow;
    state->prevDn  = dnNow;

    // --- 7. Write secOutValue (runner commits to SUBGEAR_BUS) ----------------
    proc->secOutValue = static_cast<uint16_t>(state->subGear);
}

// EOF sim_subgear_btn.cpp
