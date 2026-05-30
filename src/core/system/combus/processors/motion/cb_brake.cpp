/******************************************************************************
 * @file  cb_brake.cpp
 * @brief CbProc functions — braking pipeline (brake-input normalize / reverse-brake /
 *        dynamic ramp deceleration).
 *****************************************************************************/

#include "cb_brake.h"

#include <Arduino.h>                                             // millis()
#include <core/system/combus/combus_res.h>                      // CbusNeutral


// =============================================================================
// 1. BRAKE-INPUT NORMALIZER
// =============================================================================

/**
 * @brief Normalize BRAKE_BUS (0..analogBusMaxVal) → CbBrakeState::brakeVal
 *        (0..CbusNeutral).
 *
 * @details analogBusMaxVal = 65535 = 2 × CbusNeutral + 1 (1-lsb asymmetry).
 *   A logical right shift by 1 maps 0..65535 → 0..32767 ≈ 0..CbusNeutral.
 *   Full brake-input deflection therefore saturates at CbusNeutral.
 *   CbBrakeCfg::brakeScalePct is applied downstream in cb_rev_brake_fn.
 */
void cb_brake_fn(CbProc* proc, uint16_t& /*value*/, bool& /*claimed*/, ChanOwner /*owner*/)
{
    auto* st = static_cast<CbBrakeState*>(proc->state);

    //  Progressive (square) curve: light press → very soft, full press → CbusNeutral.
    //  raw ∈ [0..CbusNeutral]; brakeVal = raw² / CbusNeutral.
    const uint16_t raw = static_cast<uint16_t>(proc->inValue >> 1u);
    st->brakeVal = static_cast<uint16_t>(
        (static_cast<uint32_t>(raw) * raw) / CbusNeutral);
}


// =============================================================================
// 2. REVERSE-BRAKE MERGER
// =============================================================================

/**
 * @brief Compute reverse-brake force, merge with L2, apply hold timer.
 *
 * @details
 *   Reverse-brake magnitude = opposing stick deflection from neutral:
 *     rampVal > CbusNeutral (going FWD)  AND  stickVal < CbusNeutral
 *       → revBrake = CbusNeutral − stickVal  (full reverse stick = CbusNeutral)
 *     rampVal < CbusNeutral (going REV)  AND  stickVal > CbusNeutral
 *       → revBrake = stickVal − CbusNeutral  (full forward stick ≈ CbusNeutral)
 *
 *   Post-brake hold:
 *     When revBraking was true and revBrake drops to 0 (ramp has crossed to
 *     the same side as the stick), start a timer (cfg->reverseHoldMs).
 *     While the timer is active, BRAKE_BUS is clamped to cfg->maxBrake to
 *     keep the vehicle stationary and prevent an immediate direction reversal.
 *
 *   Final result = max(l2Brake, revBrake, holdBrake), capped at maxBrake.
 */
void cb_rev_brake_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*owner*/)
{
    const auto* cfg    = static_cast<const CbBrakeCfg*>(proc->cfg);
    auto*       st     = static_cast<CbBrakeState*>(proc->state);
    const uint16_t rampVal  = value;            // post-ramp bipolar (vehicle speed proxy)
    const uint16_t stickVal = proc->inValue;    // raw throttle stick (THROTTLE_BUS)

    // -----------------------------------------------------------------
    // Step 1 — Compute reverse-brake magnitude.
    // -----------------------------------------------------------------
    uint16_t revBrake = 0u;

    if (rampVal > CbusNeutral && stickVal < CbusNeutral) {
        //  Going FWD, stick pulled to REV — proportional to stick deflection.
        const uint32_t scaled = (uint32_t)(CbusNeutral - stickVal) * cfg->revBrakeScalePct / 100u;
        revBrake = static_cast<uint16_t>(scaled < CbusNeutral ? scaled : CbusNeutral);
    } else if (rampVal < CbusNeutral && stickVal > CbusNeutral) {
        //  Going REV, stick pushed to FWD — proportional to stick deflection.
        const uint32_t scaled = (uint32_t)(stickVal - CbusNeutral) * cfg->revBrakeScalePct / 100u;
        revBrake = static_cast<uint16_t>(scaled < CbusNeutral ? scaled : CbusNeutral);
    }

    // -----------------------------------------------------------------
    // Step 2 — Post-brake hold state machine.
    // -----------------------------------------------------------------
    const uint32_t now = millis();

    //  Detect reverse-brake disengagement (ramp just crossed to stick side).
    if (st->revBraking && revBrake == 0u) {
        st->holdUntilMs = now + cfg->reverseHoldMs;
        st->revBraking  = false;
    }
    if (revBrake > cfg->revBrakeDeadBand) {
        st->revBraking = true;
    }

    const uint16_t holdBrake = (now < st->holdUntilMs) ? cfg->maxBrake : 0u;

    // -----------------------------------------------------------------
    // Step 3 — Merge sources and clamp.
    // -----------------------------------------------------------------
    //  Apply brakeScalePct: cap brake-input contribution proportionally.
    const uint16_t scaledBrakeVal = static_cast<uint16_t>(
        (static_cast<uint32_t>(st->brakeVal) * cfg->brakeScalePct) / 100u);

    uint16_t finalBrake = revBrake;
    if (scaledBrakeVal > finalBrake) finalBrake = scaledBrakeVal;
    if (holdBrake      > finalBrake) finalBrake = holdBrake;
    if (finalBrake     > cfg->maxBrake) finalBrake = cfg->maxBrake;

    proc->outValue = finalBrake;    // runner commits to BRAKE_BUS

    // -----------------------------------------------------------------
    // Step 4 — Write extBrakeSteps: proportional live modifier for the ramp.
    // -----------------------------------------------------------------
    //  Linear map: finalBrake 0..maxBrake → extBrakeSteps 0..maxBrakeStep.
    //  cb_ramp_fn adds this to the static brakeSteps (coasting baseline) each tick.
    //  When brake is released extBrakeSteps → 0; brakeSteps reverts to coasting.
    if (cfg->dynRampCfg != nullptr) {
        cfg->dynRampCfg->extBrakeSteps = static_cast<uint16_t>(
            (static_cast<uint32_t>(cfg->maxBrakeStep) * finalBrake) / CbusNeutral);
    }
    (void)value;                    // observer — does not modify pipeline
}


// EOF cb_brake.cpp
