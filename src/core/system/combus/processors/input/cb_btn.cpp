/*!****************************************************************************
 * @file  cb_btn.cpp
 * @brief CbProc button functions — implementation.
 *******************************************************************************
 */

#include "cb_btn.h"

#include <Arduino.h>  // millis()


// =============================================================================
// INTERNAL HELPERS
// =============================================================================

/**
 * @brief Detect rising edge + debounce + long-press guard.
 *
 * @param pressed      Current button state (true = pressed).
 * @param state        Button state tracker.
 * @param cfg          Button config.
 * @return true if the button condition is met (rising edge or hold threshold).
 */
static inline bool btn_should_fire(bool pressed, CbBtnState* state, const CbBtnCfg* cfg) {
    const uint32_t now = millis();
    bool risingEdge = false;

    // Rising edge detection
    if (pressed && !state->prevPressed) {
        // Debounce guard
        if (cfg->debounceMs > 0 && (now - state->lastPressMs) < cfg->debounceMs) {
            state->prevPressed = pressed;
            return false;
        }
        state->lastPressMs = now;
        state->holdFired = false;  // Reset hold flag on new press
        risingEdge = true;
    }

    state->prevPressed = pressed;

    // No hold configured → fire exactly once on rising edge.
    // NOTE: the previous `return pressed && elapsed < 50` was wrong — it fired
    // multiple times per press (once per loop cycle within the 50 ms window).
    if (cfg->holdMs == 0) {
        return risingEdge;
    }

    // Long-press mode
    if (!pressed) {
        state->holdFired = false;  // Reset when button released
        return false;
    }

    // Button is held — check if threshold reached and not already fired
    if (!state->holdFired && (now - state->lastPressMs) >= cfg->holdMs) {
        state->holdFired = true;
        return true;
    }

    return false;
}


// =============================================================================
// PUBLIC API — 4 FUNCTIONS
// =============================================================================

void cb_btn_push_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)chainOwner;

    const auto* cfg   = static_cast<const CbBtnCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnState*>(proc->state);

    const bool pressed = proc->inValue[0];

    if (btn_should_fire(pressed, state, cfg)) {
        value   = cfg->bound;
        claimed = true;
    }
}

void cb_btn_inc_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)chainOwner;

    const auto* cfg   = static_cast<const CbBtnCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnState*>(proc->state);

    const bool pressed = proc->inValue[0];

    if (btn_should_fire(pressed, state, cfg)) {
        if (value < cfg->bound) {
            value++;
        }
        claimed = true;
    }
}

void cb_btn_dec_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)chainOwner;

    const auto* cfg   = static_cast<const CbBtnCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnState*>(proc->state);

    const bool pressed = proc->inValue[0];

    if (btn_should_fire(pressed, state, cfg)) {
        if (value > cfg->bound) {
            value--;
        }
        claimed = true;
    }
}

void cb_btn_toggle_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)chainOwner;

    const auto* cfg   = static_cast<const CbBtnCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnState*>(proc->state);

    const bool pressed = proc->inValue[0];

    if (btn_should_fire(pressed, state, cfg)) {
        value   = (value > 0) ? 0 : cfg->bound;
        claimed = true;
    }
}


// EOF cb_btn.cpp
