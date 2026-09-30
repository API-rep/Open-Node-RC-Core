/******************************************************************************
 * @file  cb_runlevel.cpp
 * @brief CbProc — RunLevel setters (continuous + edge-triggered) — implementation.
 ******************************************************************************/

#include "cb_runlevel.h"


// =============================================================================
// 1. CONTINUOUS (STATELESS) VARIANT
// =============================================================================

void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed) {
    const auto* cfg = static_cast<const CbRunlevelCfg*>(proc->cfg);

    const bool lever = (proc->inValue != 0u);

    // --- Select branch and write value if defined ---
    bool wrote = false;
    if (lever) {
        if (cfg->high.has_value()) {
            value = static_cast<uint16_t>(cfg->high.value());
            wrote = true;
        }
    } else {
        if (cfg->low.has_value()) {
            value = static_cast<uint16_t>(cfg->low.value());
            wrote = true;
        }
    }

    // --- Claim only if a write actually occurred ---
    if (wrote && cfg->claim) {
        claimed = true;
    }
}


// =============================================================================
// 2. EDGE-TRIGGERED (STATEFUL) VARIANT
// =============================================================================

void cb_runlevel_once_fn(CbProc* proc, uint16_t& value, bool& claimed) {
    const auto* cfg   = static_cast<const CbRunlevelCfg*>(proc->cfg);
    auto*       state = static_cast<CbRunlevelOnceState*>(proc->state);

    const uint16_t cur = proc->inValue;

    // --- No transition: do nothing, just update prevValue and exit ---
    if (cur == state->prevValue) {
        state->prevValue = cur;
        return;
    }

    // --- Transition detected: apply the same logic as cb_runlevel_fn ---
    const bool lever = (cur != 0u);

    bool wrote = false;
    if (lever) {
        if (cfg->high.has_value()) {
            value = static_cast<uint16_t>(cfg->high.value());
            wrote = true;
        }
    } else {
        if (cfg->low.has_value()) {
            value = static_cast<uint16_t>(cfg->low.value());
            wrote = true;
        }
    }

    if (wrote && cfg->claim) {
        claimed = true;
    }

    // --- Always update prevValue on exit ---
    state->prevValue = cur;
}


// EOF cb_runlevel.cpp