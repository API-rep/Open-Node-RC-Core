/******************************************************************************
 * @file  gear_fsm.cpp
 * @brief Virtual gearbox FSM primitives — implementation.
 *****************************************************************************/

#include "gear_fsm.h"

#include <Arduino.h>   // millis()


// =============================================================================
// FSM PRIMITIVES
// =============================================================================

void gear_fsm_init(GearFsmState* state)
{
    state->gear           = 1;
    state->prevRpm        = 0;
    state->lastShiftMs    = 0u;
    state->subGear        = 0;
    state->prevSubGearSet = false;
    state->prevSubGearUp  = false;
    state->prevSubGearDn  = false;
}

int8_t gear_fsm_update(GearFsmState*           state,
                       const GearShiftProfile& profile,
                       int16_t                 rpm)
{
    // Reverse or standing: force gear 1.
    if (rpm <= 0) {
        state->gear    = 1;
        state->prevRpm = rpm;
        return state->gear;
    }

    const uint32_t now        = millis();
    const uint32_t elapsed    = now - state->lastShiftMs;
    const bool     decreasing = (rpm < state->prevRpm);

    // Upshift: rising RPM above threshold, guard elapsed.
    if (state->gear < static_cast<int8_t>(profile.gearCount)
        && rpm     > profile.gear[state->gear - 1].upShift
        && !decreasing
        && elapsed > profile.shiftGuardMs)
    {
        state->gear++;
        state->lastShiftMs = now;
    }
    // Downshift: falling RPM below hysteresis threshold, guard elapsed.
    else if (state->gear > 1 && elapsed > profile.shiftGuardMs)
    {
        const int16_t thr = decreasing
                          ? profile.gear[state->gear - 1].downShiftBraking
                          : profile.gear[state->gear - 1].downShift;
        if (rpm < thr) {
            state->gear--;
            state->lastShiftMs = now;
        }
    }

    state->prevRpm = rpm;
    return state->gear;
}

// EOF gear_fsm.cpp
