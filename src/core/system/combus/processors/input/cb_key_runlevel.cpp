/******************************************************************************
 * @file  cb_key_runlevel.cpp
 * @brief CbProc — ignition key → RunLevel bridge — implementation.
 *****************************************************************************/

#include "cb_key_runlevel.h"

#include <Arduino.h>  // millis()
#include <core/system/combus/combus_access.h>  // combus_set_runlevel()


// =============================================================================
// 1. PROCESSOR FUNCTION
// =============================================================================

void cb_key_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)claimed;  // KEY_ACTIVE always updated — no bypass semantics needed here.

    const auto* cfg   = static_cast<const CbKeyRunlevelCfg*>(proc->cfg);
    auto*       state = static_cast<CbKeyRunlevelState*>(proc->state);

    const bool    pressed  = (proc->inValue != 0u);
    const uint32_t now     = millis();
    const RunLevel rl      = state->bus->runLevel;

    // --- 1. Rising-edge: IDLE / SLEEPING → STARTING ---
    if (pressed && !state->prevPressed) {
        if (rl == RunLevel::IDLE || rl == RunLevel::SLEEPING) {
            combus_set_runlevel(*state->bus, RunLevel::STARTING, chainOwner);
            state->keyActive = true;
        }
          // Defer hold-timer arming to the first RUNNING cycle (section 2).
          // Setting 0 here ensures section 2 never mistakes a stale timestamp.
        state->holdStartMs = 0u;
        state->holdFired   = false;
    }

    // --- 2. Hold detection (RUNNING → IDLE shutdown) ---
    //   Timer is armed on the first RUNNING cycle while the button is held —
    //   whether that press started in RUNNING or carried over from STARTING.
    //   This prevents the start-press time from counting toward the 3 s hold.
    if (pressed) {
        if (rl == RunLevel::RUNNING) {
            if (state->holdStartMs == 0u) {
                  // Arm: first cycle where RUNNING + pressed (and timer not already running).
                state->holdStartMs = now;
            } else if (!state->holdFired
                       && (now - state->holdStartMs) >= cfg->shutdownHoldMs) {
                combus_set_runlevel(*state->bus, RunLevel::IDLE, chainOwner);
                state->keyActive   = false;
                state->holdFired   = true;
            }
        }
    } else {
          // Button released — disarm hold timer, ready for next press.
        state->holdStartMs = 0u;
        state->holdFired   = false;
    }

    state->prevPressed = pressed;

    // --- 3. Output: persistent KEY_ACTIVE state ---
    value = state->keyActive ? 1u : 0u;
}


// EOF cb_key_runlevel.cpp
