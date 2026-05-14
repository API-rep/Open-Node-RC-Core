/******************************************************************************
 * @file  gear_fsm.cpp
 * @brief Speed-based virtual N-gear FSM — implementation.
 *****************************************************************************/

#include "gear_fsm.h"

#include <Arduino.h>    // millis()


// =============================================================================
// 1. LIFECYCLE
// =============================================================================

void gear_fsm_init(GearFsmState* s)
{
    s->gear        = 1;
    s->lastShiftMs = 0u;
}


// =============================================================================
// 2. UPDATE
// =============================================================================

int8_t gear_fsm_update(GearFsmState*            s,
                       const GearShiftProfile&  profile,
                       int16_t                  rpm,
                       uint8_t              throttlePct,
                       bool                 isBraking,
                       bool                 isReverse)
{
      // Reverse: force gear 1, skip further processing
    if (isReverse) {
        s->gear = 1;
        return s->gear;
    }

    const uint32_t now        = millis();
    const uint32_t shiftGuard = now - s->lastShiftMs;

      // Upshift: speed above threshold, sufficient throttle, guard elapsed
    if (s->gear < static_cast<int8_t>(profile.gears)
        && rpm         > profile.upShift[s->gear - 1]
        && throttlePct > profile.throttleGuardPct
        && shiftGuard  > profile.shiftGuardMs)
    {
        s->gear++;
        s->lastShiftMs = now;
    }
      // Downshift: speed below hysteresis threshold, guard elapsed
    else if (s->gear > 1 && shiftGuard > profile.shiftGuardMs)
    {
        const int16_t threshold = isBraking
                                ? profile.downShiftBraking[s->gear - 1]
                                : profile.downShift[s->gear - 1];
        if (rpm < threshold) {
            s->gear--;
            s->lastShiftMs = now;
        }
    }

    return s->gear;
}


// EOF gear_fsm.cpp
