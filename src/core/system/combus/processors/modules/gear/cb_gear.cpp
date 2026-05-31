/******************************************************************************
 * @file  cb_gear.cpp
 * @brief Virtual gearbox CbProc wrappers â€” implementation.
 *****************************************************************************/

#include "cb_gear.h"

#include "gear_fsm.h"  // gear_fsm_init, gear_fsm_update

#include <core/system/combus/combus_res.h>                     // CbusNeutral, CbusMaxVal
#include <struct/combus/processors/motion/cb_ramp_struct.h>    // CbRampCfg
#include <struct/simulation_struct.h>                          // DriveStateBus

#include <Arduino.h>   // constrain()


// =============================================================================
// 1. FSM WRAPPER
// =============================================================================

void gear_fsm_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg* cfg   = static_cast<const GearProcCfg*>(proc->cfg);
    GearFsmState*      state = static_cast<GearFsmState*>(proc->state);

    // --- Self-init on first call (gear == 0 = zero-initialised state) -------
    if (state->gear == 0) {
        gear_fsm_init(state);
    }

    // --- RPM magnitude from ESC_RPM_BUS + DRIVE_STATE_BUS gate -------------------
    //     inCh = DRIVE_STATE_BUS (analog). Runner pre-reads into inValue.
    const int8_t ds = DriveStateBus::decode(proc->inValue);

    // --- Run gear FSM / resolve gear output ----------------------------------
    int8_t gear;
    if (ds == DriveState::kStanding) {
        // At rest: output neutral gear (0). FSM internal state preserved for
        // smooth resume (state->gear stays at 1 — last engaged gear).
        gear = 0;
    } else {
        // Active drive or reverse: pass RPM to FSM.
        // ds > 0 (forward/braking-fwd): actual engine_rpm after gear_ratio_inv_fn.
        // ds < 0 (reverse/braking-rev): rpm = 0 → FSM stays at gear 1.
        const int16_t rpm = (ds > 0) ? static_cast<int16_t>(value) : int16_t(0);
        gear = gear_fsm_update(state, *cfg->profile, rpm);
    }

    // --- Output — gear integer becomes the channel value ---------------------
    value = static_cast<uint16_t>(gear);
}



// =============================================================================
// 2. INVERSE GEAR RATIO  (wheel-speed → engine RPM)
// =============================================================================

/**
 * @brief wheel_speed × 1000 / gearRatio[prevGear] = engine_rpm.
 *
 * @details Placed FIRST in the GEAR chain (before FSM) in wheel-speed-primary mode.
 *   Reads `value` = wheel_speed (seeded from ESC_RPM_BUS by the `in` proc).
 *   Reads `state->gear` (GearFsmState) from the PREVIOUS tick — 1-cycle latency,
 *   same as gear_dyn_ramp_fn.  Gear 0 (uninitialised) → passthrough.
 *
 *   Side-effect: writes engine_rpm to `proc->outValue` so the chain runner
 *   commits it to `proc->outCh` (= ESC_RPM_BUS) — the sound node reads RPM from there.
 *
 *   Also passes engine_rpm downstream in `value` for gear_fsm_fn so RPM
 *   thresholds work in engine-RPM domain (unchanged from old model).
 *
 *   state = GearFsmState* (shared with gear_fsm_fn, read-only here).
 */
void gear_ratio_inv_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    const GearFsmState*     state   = static_cast<const GearFsmState*>(proc->state);

    // state->gear is from the previous tick (written by gear_fsm_fn last cycle).
    // Gear 0 = FSM not yet initialised — passthrough.
    if (state->gear <= 0) {
        proc->outValue = value;
        return;
    }

    const uint8_t gearIdx = static_cast<uint8_t>(
        constrain(static_cast<int>(state->gear) - 1, 0, static_cast<int>(profile->gearCount) - 1));
    const uint16_t ratio = profile->gear[gearIdx].gearRatio;

    if (ratio == 0u) {
        proc->outValue = value;
        return;
    }

    // engine_rpm = wheel_speed × 1000 / gearRatio.
    // At upshift: gear changes next tick → engine_rpm drops automatically (ratio increases).
    const uint32_t engineRpm = static_cast<uint32_t>(value) * 1000u
                             / static_cast<uint32_t>(ratio);
    const uint16_t result    = engineRpm > 0xFFFFu
                               ? uint16_t(0xFFFFu)
                               : static_cast<uint16_t>(engineRpm);

    // Commit engine_rpm to outCh (ESC_RPM_BUS) — sound node reads RPM from there.
    proc->outValue = result;

    // Pass engine_rpm downstream so gear_fsm_fn compares against engine-RPM thresholds.
    value = result;
}


// =============================================================================
// 3. RPM × GEAR RATIO (MULTIPLICATIVE — forward, RPM-primary mode)
// =============================================================================

/** @brief Scale RPM magnitude by gearRatio[gear] / 1000 -- stays in RPM domain. */
void gear_ratio_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    const uint8_t           nGears  = profile->gearCount;

    // inCh = GEAR (analog).
    const int8_t rawGear = static_cast<int8_t>(proc->inValue);

    if (rawGear == 0) return;  // GEAR=0 sentinel â€” passthrough.

    const uint8_t gearIdx = static_cast<uint8_t>(
        constrain(static_cast<int>(rawGear) - 1, 0, static_cast<int>(nGears) - 1));

    // value x gearRatio/1000 -- stays in RPM domain for gear_dir_fn.
    const uint32_t scaled = static_cast<uint32_t>(value)
                          * static_cast<uint32_t>(profile->gear[gearIdx].gearRatio)
                          / 1000u;
    value = scaled > 0xFFFFu ? uint16_t(0xFFFFu) : static_cast<uint16_t>(scaled);
}


// =============================================================================
// 4. SUB-GEAR SPEED CAP
// =============================================================================

/** @brief Cap RPM magnitude to maxSpeedPct when sub-gear is active. */
void gear_subgear_cap_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    // inCh = SUBGEAR_BUS (0 = inactive, 1..N = sub-gear index).
    const uint8_t subIdx = static_cast<uint8_t>(proc->inValue);
    if (subIdx == 0u) return;  // Normal mode â€” passthrough.

    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;

    if (profile->subGear == nullptr || profile->subGearCount == 0u) {
        value = 0u;
        return;
    }

    // maxAbsRpm = top gear upShift — same denominator as gear_dir_fn.
    const uint16_t maxAbsRpm = profile->gear[profile->gearCount - 1u].upShift;

    // Clamp sub-gear index (1-based â†’ 0-based).
    const uint8_t gi = static_cast<uint8_t>(
        constrain(static_cast<int>(subIdx) - 1, 0, static_cast<int>(profile->subGearCount) - 1));

    // cappedRpm = maxAbsRpm Ã— maxSpeedPct / 100 â€” cap in RPM domain.
    const uint32_t cappedRpm = static_cast<uint32_t>(maxAbsRpm)
                             * static_cast<uint32_t>(profile->subGear[gi].maxSpeedPct) / 100u;
    if (value > static_cast<uint16_t>(cappedRpm))
        value = static_cast<uint16_t>(cappedRpm);
}


// =============================================================================
// 5. MAGNITUDE â†’ BIPOLAR (DIRECTION APPLICATION)
// =============================================================================

/** @brief Scale RPM magnitude to ComBus half range and apply direction. */
void gear_dir_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    const uint8_t           nGears  = profile->gearCount;

    // maxAbsRpm — denominator for the ComBus half scale.
    const uint16_t maxAbsRpm = profile->gear[nGears - 1u].upShift;

    // inCh = DRIVE_STATE_BUS (analog).
    const int8_t ds = DriveStateBus::decode(proc->inValue);

    if (maxAbsRpm == 0u || value == 0u) { value = CbusNeutral; return; }

    const uint32_t half = static_cast<uint32_t>(value)
                        * static_cast<uint32_t>(CbusNeutral)
                        / static_cast<uint32_t>(maxAbsRpm);

    if (ds > 0) {
        value = static_cast<uint16_t>(constrain(
                    static_cast<int32_t>(CbusNeutral) + static_cast<int32_t>(half),
                    0, static_cast<int32_t>(CbusMaxVal)));
    } else if (ds < 0) {
        value = static_cast<uint16_t>(constrain(
                    static_cast<int32_t>(CbusNeutral) - static_cast<int32_t>(half),
                    0, static_cast<int32_t>(CbusMaxVal)));
    } else {
        value = CbusNeutral;
    }
}


// =============================================================================
// 6. GEAR → RAMP BRIDGE
// =============================================================================

void gear_dyn_ramp_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    if (proc->dynCfg == nullptr) return;  // No ramp linked â€” passthrough.

    const GearProcCfg* cfg = static_cast<const GearProcCfg*>(proc->cfg);
    CbRampCfg*        dyn = static_cast<CbRampCfg*>(proc->dynCfg);

    // --- Resolve gear from value (= gear after claim cascade) ---------------
    const int8_t gear = static_cast<int8_t>(value);

    // --- Look up ramp time for this gear ------------------------------------
    const uint8_t gi = static_cast<uint8_t>(
        constrain(static_cast<int>(gear) - 1,
                  0, static_cast<int>(cfg->profile->gearCount) - 1));
    const uint16_t newRampTime = cfg->profile->gear[gi].rampTime;

    // --- Write dynCfg only on change -----------------------------------------
    if (dyn->rampTimeMs != newRampTime) {
        dyn->rampTimeMs = newRampTime;
        dyn->resetRamp  = true;
    }
    // value (= gear) passed through unchanged.
}

// =============================================================================
// 7. UPSHIFT FREEZE
// =============================================================================

/**
 * @brief Detect upshift; freeze traction ramp for upshiftDampMs + signal GEAR_SHIFTING.
 *
 * @details Placed in GEAR chain (after gear_dyn_ramp_fn, before out).
 *   Reads current gear from `value` (= gear computed in this GEAR-chain pass).
 *   On upshift (curGear > prevGear, excluding first-init): arms a timer and
 *   sets dynCfg->rampTimeMs = UINT16_MAX — the ramp proc never ticks while
 *   this value is set, freezing RPM at the upshift point.
 *   gear_dyn_ramp_fn (runs before this proc in the GEAR chain) detects
 *   UINT16_MAX ≠ gear rampTime on the first cycle after expiry and restores
 *   the correct value + sets resetRamp = true automatically.
 *   Does NOT touch extAccelSteps or extBrakeSteps.
 *   Sets proc->outValue = 1 while freeze active, 0 otherwise;
 *   runner commits outValue to outCh = GEAR_SHIFTING (machine-local digital).
 *   Does NOT modify the pipeline `value` (gear passes through unchanged).
 *
 *   Rationale for GEAR chain placement: if the gear chain is disabled,
 *   no upshift occurs and rampTimeMs is never overridden.
 *
 *   cfg    = GearProcCfg*    (profile — upshiftDampMs).
 *   dynCfg = CbRampCfg*      (traction ramp — rampTimeMs frozen here).
 *   state  = GearDampState*  (prevGear + dampEndMs).
 *   outCh  = GEAR_SHIFTING   (machine-local digital).
 *   @todo winter 2026: promote GEAR_SHIFTING to WIRE region so sound node reads directly.
 */
void gear_upshift_damp_fn(CbProc* proc, uint16_t& value,
                           bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    if (proc->dynCfg == nullptr) return;  // No ramp linked — passthrough.

    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    GearDampState*          state   = static_cast<GearDampState*>(proc->state);
    CbRampCfg*              ramp    = static_cast<CbRampCfg*>(proc->dynCfg);

    // In GEAR chain: read current gear from pipeline value (= gear after claim cascade).
    const uint8_t  curGear = static_cast<uint8_t>(value);
    const uint32_t now     = millis();

    // Upshift detected (prevGear > 0 guards against first-init 0→1 pseudo-shift).
    if (curGear > state->prevGear && state->prevGear > 0u) {
        state->rpmAtShift  = state->lastRpm;   // snapshot from gear_upshift_rpm_fade_fn (prev cycle)
        state->dampStartMs = now;
        state->dampEndMs   = now + static_cast<uint32_t>(profile->upshiftDampMs);
    }

    // Freeze: hold rampTimeMs at UINT16_MAX while window is active so the ramp
    // proc never ticks.  gear_dyn_ramp_fn restores the correct value next cycle
    // after expiry (it detects UINT16_MAX ≠ gear rampTime and sets resetRamp).
    if (state->dampEndMs > 0u) {
        if (now >= state->dampEndMs) {
            state->dampEndMs = 0u;
            // gear_dyn_ramp_fn will restore rampTimeMs and set resetRamp = true next cycle.
        } else {
            ramp->rampTimeMs = UINT16_MAX;
        }
    }

    state->prevGear = curGear;

    // Signal via ComBus — runner commits outValue to outCh = GEAR_SHIFTING.
    proc->outValue = (state->dampEndMs > 0u) ? 1u : 0u;
}


// =============================================================================
// 8. RPM FADE  (upshift interpolation on ESC_RPM_BUS)
// =============================================================================

/**
 * @brief Smoothly interpolate ESC_RPM_BUS from rpmAtShift to natural engine_rpm
 *        over the upshift damp window.
 *
 * @details Placed AFTER gear-inv-ratio and BEFORE gear-fsm in the GEAR chain.
 *   Every cycle: records `state->lastRpm = value` (= natural engine_rpm from
 *   gear_ratio_inv_fn) so that gear_upshift_damp_fn can snapshot it as
 *   `rpmAtShift` at the moment of an upshift.
 *
 *   When a damp window is active (`dampEndMs > 0`):
 *     - Computes elapsed = now − dampStartMs.
 *     - If elapsed < upshiftDampMs: writes linearly interpolated RPM to
 *       proc->outValue → runner commits to outCh = ESC_RPM_BUS, overriding
 *       gear-inv-ratio's earlier write.  Does NOT modify `value` (gear_fsm_fn
 *       downstream needs the true natural engine_rpm for shift decisions).
 *     - If elapsed >= upshiftDampMs: window has ended — falls through to
 *       pass-through (gear_upshift_damp_fn will clear dampEndMs this cycle).
 *
 *   Outside any window: pass-through — writes `value` to proc->outValue so
 *   this proc remains the authoritative writer of ESC_RPM_BUS (gear-inv-ratio
 *   still runs first but its outValue is superseded).
 *
 *   cfg   = GearProcCfg*   (needs profile->upshiftDampMs).
 *   state = GearDampState* (shared with gear_upshift_damp_fn).
 *   outCh = ESC_RPM_BUS.
 */
void gear_upshift_rpm_fade_fn(CbProc* proc, uint16_t& value,
                      bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    GearDampState*          state   = static_cast<GearDampState*>(proc->state);

    // Record natural RPM every cycle — used as rpmAtShift snapshot by gear_upshift_damp_fn.
    state->lastRpm = static_cast<uint16_t>(value);

    // Interpolate while a damp window is active.
    if (state->dampEndMs > 0u && profile->upshiftDampMs > 0u) {
        const uint32_t elapsed  = millis() - state->dampStartMs;
        const uint32_t duration = static_cast<uint32_t>(profile->upshiftDampMs);

        if (elapsed < duration) {
            const int32_t delta  = static_cast<int32_t>(value)
                                 - static_cast<int32_t>(state->rpmAtShift);
            const int32_t interp = static_cast<int32_t>(state->rpmAtShift)
                                 + delta * static_cast<int32_t>(elapsed)
                                 / static_cast<int32_t>(duration);
            proc->outValue = static_cast<uint16_t>(constrain(interp, 0,
                                                              static_cast<int32_t>(CbusMaxVal)));
            return;
        }
    }

    // Outside damp window: mirror gear-inv-ratio's natural RPM output.
    proc->outValue = value;
}

// EOF cb_gear.cpp
