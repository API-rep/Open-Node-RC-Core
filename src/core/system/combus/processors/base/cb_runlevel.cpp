/******************************************************************************
 * @file  cb_runlevel.cpp
 * @brief CbProc — ComBus RunLevel writer — implementation.
 *****************************************************************************/

#include "cb_runlevel.h"

#include <core/system/combus/combus_access.h>   // combus_set_runlevel()


// =============================================================================
// 1. PROCESSOR FUNCTION
// =============================================================================

void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner) {
    (void)value;    // pass-through — intentionally not modified.
    (void)claimed;  // never claims the channel.

    const auto* cfg   = static_cast<const CbRunlevelCfg*>(proc->cfg);
    auto*       state = static_cast<CbRunlevelState*>(proc->state);

    const bool     active = (proc->inValue != 0u);
    const RunLevel rl     = state->bus->runLevel;

    // --- Rising edge: activate ---
    if (active && !state->prevValue) {
        if (rl == RunLevel::IDLE || rl == RunLevel::SLEEPING) {
            combus_set_runlevel(*state->bus, cfg->activeLevel, chainOwner);
        }
    }

    // --- Falling edge: deactivate ---
    if (!active && state->prevValue) {
        if (rl == RunLevel::STARTING || rl == RunLevel::RUNNING) {
            combus_set_runlevel(*state->bus, cfg->defaultLevel, chainOwner);
        }
    }

    state->prevValue = active;
}


// EOF cb_runlevel.cpp
