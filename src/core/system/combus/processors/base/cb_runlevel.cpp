/******************************************************************************
 * @file  cb_runlevel.cpp
 * @brief CbProc — ComBus RunLevel writer — implementation.
 *****************************************************************************/

#include "cb_runlevel.h"

#include <core/system/combus/combus_access.h>   // combus_set_analog()


// =============================================================================
// 1. PROCESSOR FUNCTION
// =============================================================================

void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed) {
    (void)value;    // pass-through — intentionally not modified.
    (void)claimed;  // never claims the channel.

    const auto* cfg   = static_cast<const CbRunlevelCfg*>(proc->cfg);
    auto*       state = static_cast<CbRunlevelState*>(proc->state);

    const bool     active = (proc->inValue != 0u);
    // RL3: runLevel is now a plain analog channel — read from analogBus[RUNLEVEL].
    const RunLevel rl     = (RunLevel)state->bus->analogBus[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)].value;

    // --- Rising edge: activate ---
    if (active && !state->prevValue) {
        if (rl == RunLevel::IDLE || rl == RunLevel::SLEEPING) {
            // RL3: write via generic analog accessor (no dedicated runLevel API).
            combus_set_analog(*state->bus, AnalogComBusID::RUNLEVEL, (uint16_t)cfg->activeLevel, ChanLayer::LOCAL);
        }
    }

    // --- Falling edge: deactivate ---
    if (!active && state->prevValue) {
        if (rl == RunLevel::STARTING || rl == RunLevel::RUNNING) {
            // RL3: write via generic analog accessor (no dedicated runLevel API).
            combus_set_analog(*state->bus, AnalogComBusID::RUNLEVEL, (uint16_t)cfg->defaultLevel, ChanLayer::LOCAL);
        }
    }

    state->prevValue = active;
}


// EOF cb_runlevel.cpp
