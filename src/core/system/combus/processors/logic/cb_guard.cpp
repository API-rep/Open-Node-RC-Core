/*!****************************************************************************
 * @file  cb_guard.cpp
 * @brief CbProc guard — implementation.
 *******************************************************************************
 */
#include "cb_guard.h"
#include <Arduino.h>  // millis()


void cb_guard_fn(CbProc* proc, uint16_t& value, bool& claimed)
{
    const auto* cfg   = static_cast<const CbGuardCfg*>(proc->cfg);
    auto*       state = static_cast<CbGuardState*>(proc->state);
    const uint32_t now = millis();
    const bool     on  = (proc->inValue != 0u);

        // --- Settle time counted from the last rising edge of the input ---
    if (on && !state->wasOn) { state->onSinceMs = now; }
    state->wasOn = on;

    if (cfg->whileZeroOnly && value != 0u) { return; }      // a release is never guarded
    const bool open = on && (now - state->onSinceMs) >= (uint32_t)cfg->settleMs;
    if (!open) { claimed = true; }
}

// EOF cb_guard.cpp
