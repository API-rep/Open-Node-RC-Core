/******************************************************************************
 * @file  gear_fsm.h
 * @brief Virtual gearbox FSM primitives — pure RPM → gear logic.
 *
 * @details No ComBus dependency — can be used standalone or embedded in a CbProc.
 *   Exported functions:
 *   - `gear_fsm_init`   — reset FSM to gear 1
 *   - `gear_fsm_update` — advance FSM by one cycle (RPM → gear)
 *****************************************************************************/
#pragma once

#include <struct/combus/processors/modules/gear_struct.h>  // GearFsmState, GearShiftProfile


/**
 * @brief Reset a `GearFsmState` to gear 1, clearing shift guard and sub-gear.
 *
 * @details Called automatically by `gear_fsm_fn` on first call when
 *   `state->gear == 0` (zero-init detection).  Exposed here for callers that
 *   need to force-reset a running FSM (e.g. on runlevel transition).
 *
 * @param state  Non-null pointer to the FSM state to reset.
 */
void gear_fsm_init(GearFsmState* state);

/**
 * @brief Advance the N-gear FSM by one cycle.
 *
 * @details Compares `rpm` to the shift thresholds in `profile`:
 *   - Rising RPM trend above `gear[n].upShift`  → upshift by 1 (guarded).
 *   - Falling RPM below threshold → multi-step downshift: scans from current
 *     gear downward and lands on the first gear whose threshold is satisfied,
 *     skipping intermediate gears in a single call if warranted.  Both
 *     `gear[n].downShiftBraking` (decelerating) and `gear[n].downShift`
 *     (coasting) are supported; the `decreasing` flag is evaluated once.
 *   - `rpm <= 0` forces gear 1 immediately (reverse or standing).
 *
 * @param state    Mutable FSM state (gear, prevRpm, lastShiftMs).
 * @param profile  Read-only shift thresholds and guard timing.
 * @param rpm      Current RPM (positive = forward, ≤ 0 = standing/reverse).
 * @return         Active gear after this update (1–`profile.gearCount`).
 */
int8_t gear_fsm_update(GearFsmState*           state,
                       const GearShiftProfile& profile,
                       int16_t                 rpm);

// EOF gear_fsm.h
