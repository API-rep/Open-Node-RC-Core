/******************************************************************************
 * @file  cb_ramp.cpp
 * @brief CbProc function — single-axis inertia ramp (hydraulics, steering).
 *
 * @details Pure ComBus simulation processor — no hardware calls, no µs domain.
 *   Reads the target from `value` (seeded by proc_chain_step from
 *   CbChain::inCh).  Advances `state->currentPos` toward the target by
 *   `accelSteps` (moving away from neutral) or `brakeSteps` (moving toward
 *   neutral) on each `rampTimeMs` tick.  Writes the filtered position back
 *   to `value` every call — CbChain owns the final bus write.
 *****************************************************************************/

#include "cb_ramp.h"

#include <Arduino.h>                             // millis()
#include "core/system/combus/combus_res.h"       // CbusNeutral


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Asymmetric inertia ramp — see cb_ramp.h for full contract. */
void cb_sym_ramp_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    CbRampState*     state = static_cast<CbRampState*>(proc->state);
    // dynCfg override: a preceding proc (e.g. cb_gear_fn) may write proc->dynCfg
    // to select a per-cycle ramp config without touching flash.
    const CbRampCfg* cfg   = (proc->dynCfg != nullptr)
                                 ? static_cast<const CbRampCfg*>(proc->dynCfg)
                                 : static_cast<const CbRampCfg*>(proc->cfg);

    // --- 0. Self-init on first call ------------------------------------------
    // Sentinel: both fields are zero-initialised at construction; after init,
    // lastUpdateMs is set to millis() (always > 0 after boot).
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
    //  Reset request: a preceding proc (e.g. cb_gear_fn) may set dynCfg->resetRamp
    //  after updating rampTimeMs for the new gear.  We restart the timer without
    //  touching currentPos so the position ramp continues from where it was.
    if (proc->dynCfg != nullptr) {
        CbRampCfg* dyn = static_cast<CbRampCfg*>(proc->dynCfg);
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
        //  extAccelSteps / extBrakeSteps: signed live modifiers (positive = boost, negative = damp).
        //  Effective step floored at 1 to guarantee forward progress.
        const bool isAccel    = (targDist > currDist);
        const bool isNegDir   = (target < CbusNeutral);
        const bool useDownCfg = isAccel && isNegDir && (cfg->accelDownSteps != 0u);
        const int32_t  rawAccel       = static_cast<int32_t>(cfg->accelSteps)
                                      + static_cast<int32_t>(cfg->extAccelSteps);
        const uint16_t effectiveAccel = (rawAccel > 1) ? static_cast<uint16_t>(rawAccel) : 1u;
        const int32_t  rawBrake       = static_cast<int32_t>(cfg->brakeSteps)
                                      + static_cast<int32_t>(cfg->extBrakeSteps);
        const uint16_t effectiveBrake = (rawBrake > 1) ? static_cast<uint16_t>(rawBrake) : 1u;
        const uint16_t step = isAccel ? (useDownCfg ? cfg->accelDownSteps : effectiveAccel)
                                      : effectiveBrake;

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

// =============================================================================
// 2. UNIPOLAR RAMP
// =============================================================================

/** @brief Unipolar inertia ramp — see cb_ramp.h for full contract. */
void cb_uni_ramp_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    CbRampState*     state = static_cast<CbRampState*>(proc->state);
    const CbRampCfg* cfg   = (proc->dynCfg != nullptr)
                                 ? static_cast<const CbRampCfg*>(proc->dynCfg)
                                 : static_cast<const CbRampCfg*>(proc->cfg);

    // --- 0. Self-init on first call ------------------------------------------
    // Unipolar: 0 = stopped (not CbusNeutral).
    if (state->lastUpdateMs == 0u) {
        state->currentPos   = 0u;
        state->lastUpdateMs = millis();
        if (state->lastUpdateMs == 0u) state->lastUpdateMs = 1u;
    }

    // --- 1. Read target, apply neutral band ----------------------------------
    // Snap to 0 when target falls within the near-zero deadzone.
    uint16_t target = value;
    if (cfg->neutralBand > 0u && target <= cfg->neutralBand) {
        target = 0u;
    }

    // --- 2. Ramp tick: advance currentPos one step when timer elapses --------
    if (proc->dynCfg != nullptr) {
        CbRampCfg* dyn = static_cast<CbRampCfg*>(proc->dynCfg);
        if (dyn->resetRamp) {
            state->lastUpdateMs = millis();
            if (state->lastUpdateMs == 0u) state->lastUpdateMs = 1u;
            dyn->resetRamp = false;
        }
    }

    const uint32_t now = millis();
    if (now - state->lastUpdateMs >= cfg->rampTimeMs) {
        state->lastUpdateMs = now;

        //  Step size: accelSteps when moving away from 0 (speeding up),
        //             brakeSteps when moving toward 0 (slowing down).
        //  No accelDownSteps path — magnitude has no direction concept.
        const bool isAccel = (target > state->currentPos);

        const int32_t  rawAccel       = static_cast<int32_t>(cfg->accelSteps)
                                      + static_cast<int32_t>(cfg->extAccelSteps);
        const uint16_t effectiveAccel = (rawAccel > 1) ? static_cast<uint16_t>(rawAccel) : 1u;
        const int32_t  rawBrake       = static_cast<int32_t>(cfg->brakeSteps)
                                      + static_cast<int32_t>(cfg->extBrakeSteps);
        const uint16_t effectiveBrake = (rawBrake > 1) ? static_cast<uint16_t>(rawBrake) : 1u;
        const uint16_t step           = isAccel ? effectiveAccel : effectiveBrake;

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

// EOF cb_ramp.cpp
