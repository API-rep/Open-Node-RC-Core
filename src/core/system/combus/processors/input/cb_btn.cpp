/*!****************************************************************************
 * @file  cb_btn.cpp
 * @brief CbProc button functions — implementation.
 *******************************************************************************
 */

#include "cb_btn.h"

#include <Arduino.h>  // millis()


// =============================================================================
// INTERNAL HELPER
// =============================================================================

/**
 * @brief Evaluate trigger condition and return true exactly once per qualified press or release.
 *
 * @details Updates @p state in-place; @p state->prevPressed is set before return.
 *
 *   ON_PRESS trigger: fires on (or while) pressed once elapsed >= holdMs. Repeat-guarded
 *     by fired flag — cleared on release so each new press can fire at most once.
 *   ON_RELEASE trigger: fires on falling edge if press duration >= holdMs.
 *
 * @param pressed  Current button state (true = pressed).
 * @param state    Button runtime state.
 * @param cfg      Button config.
 * @return true when the trigger condition is satisfied.
 */
static inline bool btn_check_fire(bool pressed, CbBtnState* state, const CbBtnCfg* cfg)
{
    const uint32_t now = millis();

    // --- Rising edge: start hold timer, reset repeat guard ---
    if (pressed && !state->prevPressed) {
        state->pressMs = now;
        state->fired   = false;
    }

    bool fire = false;

    if (cfg->trigger == CbBtnTrigger::ON_PRESS) {
        // Fire once per press: immediately (holdMs==0) or while held >= holdMs.
        if (pressed && !state->fired
                    && (now - state->pressMs) >= (uint32_t)cfg->holdMs) {
            fire = true;
        }
        // Release: clear repeat guard so the next press can fire.
        if (!pressed) {
            state->fired = false;
        }
    } else {
        // ON_RELEASE: fire on falling edge if press duration >= holdMs.
        if (!pressed && state->prevPressed
                     && (now - state->pressMs) >= (uint32_t)cfg->holdMs) {
            fire = true;
        }
    }

    if (fire) {
        state->fired = true;
    }

    state->prevPressed = pressed;
    return fire;
}


// =============================================================================
// PUBLIC API — 4 FUNCTIONS
// =============================================================================

void cb_btn_push_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)chainOwner;
    const auto* cfg   = static_cast<const CbBtnCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnState*>(proc->state);
    if (btn_check_fire((bool)proc->inValue, state, cfg)) {
        value   = cfg->bound;
        claimed = true;
    }
}

void cb_btn_inc_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)chainOwner;
    const auto* cfg   = static_cast<const CbBtnCfg*>(proc->cfg);
    auto*       state = static_cast<CbBtnState*>(proc->state);
    if (btn_check_fire((bool)proc->inValue, state, cfg)) {
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
    if (btn_check_fire((bool)proc->inValue, state, cfg)) {
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
    if (btn_check_fire((bool)proc->inValue, state, cfg)) {
        value   = (value > 0u) ? 0u : cfg->bound;
        claimed = true;
    }
}


// EOF cb_btn.cpp
