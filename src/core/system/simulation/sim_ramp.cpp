/******************************************************************************
 * @file  sim_ramp.cpp
 * @brief SimProc function — single-axis inertia ramp (hydraulics, steering).
 *
 * @details Pure ComBus simulation processor — no hardware calls, no µs domain.
 *   Reads the target from `value` (seeded by sim_channel_update from
 *   SimChannel::inCh).  Advances `state->currentPos` toward the target by
 *   `accelSteps` (moving away from neutral) or `brakeSteps` (moving toward
 *   neutral) on each `rampTimeMs` tick.  Writes the filtered position back
 *   to `value` every call — SimChannel owns the final bus write.
 *****************************************************************************/

#include "sim_ramp.h"

#include <Arduino.h>          // millis()

#include "core/system/combus/combus_res.h"   // CbusNeutral


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Asymmetric inertia ramp — see sim_ramp.h for full contract. */
void sim_ramp_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chanOwner*/)
{
    SimRampState*     state = static_cast<SimRampState*>(proc->state);
    // dynCfg override: a preceding proc (e.g. sim_gear_fn) may write proc->dynCfg
    // to select a per-cycle ramp config without touching flash.
    const SimRampCfg* cfg   = (proc->dynCfg != nullptr)
                                  ? static_cast<const SimRampCfg*>(proc->dynCfg)
                                  : static_cast<const SimRampCfg*>(proc->cfg);

    // --- 0. Self-init on first call ------------------------------------------
    // Sentinel: both fields are zero-initialised at construction; after init,
    // lastUpdateMs is set to millis() (always > 0 after boot).
    // Do NOT use currentPos == 0 alone — 0 is a valid position (full negative).
    if (state->lastUpdateMs == 0u) {
        state->currentPos   = CbusNeutral;
        state->lastUpdateMs = millis();
        if (state->lastUpdateMs == 0u) state->lastUpdateMs = 1u;  // guard: millis()==0 at boot
    }

    // --- 1. Read target, apply neutral band ----------------------------------
    uint16_t target = value;
    if (cfg->neutralBand > 0u) {
        const uint16_t lo = static_cast<uint16_t>(CbusNeutral - cfg->neutralBand);
        const uint16_t hi = static_cast<uint16_t>(CbusNeutral + cfg->neutralBand);
        if (target >= lo && target <= hi) target = CbusNeutral;
    }

    // --- 2. Ramp tick: advance currentPos one step when timer elapses --------
    //  Reset request: a preceding proc (e.g. sim_gear_fn) may set dynCfg->resetRamp
    //  after updating rampTimeMs for the new gear.  We restart the timer without
    //  touching currentPos so the position ramp continues from where it was.
    if (proc->dynCfg != nullptr) {
        SimRampCfg* dyn = static_cast<SimRampCfg*>(proc->dynCfg);
        if (dyn->resetRamp) {
            state->lastUpdateMs = millis();
            if (state->lastUpdateMs == 0u) state->lastUpdateMs = 1u;
            dyn->resetRamp = false;
        }
    }
    const uint16_t rampTimeMs = cfg->rampTimeMs;

    const uint32_t now = millis();
    if (now - state->lastUpdateMs >= rampTimeMs) {
        state->lastUpdateMs = now;

        //  Step size: accelSteps when moving AWAY from neutral,
        //             brakeSteps when moving TOWARD neutral.
        //  Determined by comparing absolute distances to CbusNeutral — correct
        //  for both positive and negative deflections.
        const uint32_t currDist = (state->currentPos >= CbusNeutral)
                                  ? static_cast<uint32_t>(state->currentPos - CbusNeutral)
                                  : static_cast<uint32_t>(CbusNeutral - state->currentPos);
        const uint32_t targDist = (target >= CbusNeutral)
                                  ? static_cast<uint32_t>(target - CbusNeutral)
                                  : static_cast<uint32_t>(CbusNeutral - target);
        //  accelDownSteps: asymmetric accel for negative direction (e.g. dump descent).
        //  Active when accelDownSteps != 0 AND target is below neutral AND moving away.
        const bool isAccel    = (targDist > currDist);
        const bool isNegDir   = (target < CbusNeutral);
        const bool useDownCfg = isAccel && isNegDir && (cfg->accelDownSteps != 0u);
        const uint16_t step = isAccel ? (useDownCfg ? cfg->accelDownSteps : cfg->accelSteps)
                                      : cfg->brakeSteps;

        if (state->currentPos < target) {
            const uint16_t delta = static_cast<uint16_t>(target - state->currentPos);
            state->currentPos = (delta > step) ? state->currentPos + step : target;
        } else if (state->currentPos > target) {
            const uint16_t delta = static_cast<uint16_t>(state->currentPos - target);
            state->currentPos = (delta > step) ? state->currentPos - step : target;
        }
    }

    // --- 3. Output filtered position -----------------------------------------
    value = state->currentPos;
}

// EOF sim_ramp.cpp
