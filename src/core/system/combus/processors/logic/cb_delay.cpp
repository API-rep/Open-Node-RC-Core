/*!****************************************************************************
 * @file  cb_delay.cpp
 * @brief CbProc delay — implementation.
 *******************************************************************************
 */
#include "cb_delay.h"
#include <Arduino.h>  // millis()


void cb_delay_fn(CbProc* proc, uint16_t& value, bool& claimed)
{
    const auto* cfg   = static_cast<const CbDelayCfg*>(proc->cfg);
    auto*       state = static_cast<CbDelayState*>(proc->state);
    const uint32_t now = millis();
    const bool     on  = (proc->inValue != 0u);

        // --- Delay counted from the last rising edge of the input ---
    if (on && !state->wasOn) { state->onSinceMs = now; }
    state->wasOn = on;

    if (cfg->whileZeroOnly && value != 0u) { return; }      // a release is never held
    const bool open = on && (now - state->onSinceMs) >= (uint32_t)cfg->delayMs;
    if (!open) { claimed = true; }
}

// EOF cb_delay.cpp
