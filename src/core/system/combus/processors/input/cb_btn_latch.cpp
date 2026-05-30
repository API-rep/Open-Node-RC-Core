/******************************************************************************
 * @file  cb_btn_latch.cpp
 * @brief CbProc — latch button processor — implementation.
 *****************************************************************************/

#include "cb_btn_latch.h"

#include <Arduino.h>  // millis()


// =============================================================================
// 1. PROCESSOR FUNCTION
// =============================================================================

void cb_btn_latch_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner /*chainOwner*/) {
    (void)claimed;  // output always written; bypass semantics not applicable here.

    const auto* cfg   = static_cast<const CbBtnLatchCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnLatchState*>(proc->state);

    const bool     pressed = (proc->inValue != 0u);
    const uint32_t now     = millis();

    // --- 1. Rising-edge detection ---
    if (pressed && !state->prevPressed) {
        if (!state->latch) {
              // OFF → arm latch ON; do not arm hold timer yet.
              // The hold timer is only meaningful when already ON — arming here
              // would let a start-press unintentionally count toward hold-off.
            state->latch       = true;
            state->holdStartMs = 0u;
            state->holdFired   = false;
        } else {
              // Already ON — arm hold timer for potential clear-on-hold.
            state->holdStartMs = now;
            state->holdFired   = false;
        }
    }

    // --- 2. Hold detection: latch ON → OFF when held long enough ---
    if (pressed && state->latch && state->holdStartMs > 0u && !state->holdFired) {
        if ((now - state->holdStartMs) >= cfg->holdMs) {
            state->latch     = false;
            state->holdFired = true;
        }
    }

    // --- 3. Release: disarm hold timer ---
    if (!pressed) {
        state->holdStartMs = 0u;
        state->holdFired   = false;
    }

    state->prevPressed = pressed;

    // --- 4. Output: persistent latch state ---
    value = state->latch ? 1u : 0u;
}


// EOF cb_btn_latch.cpp
