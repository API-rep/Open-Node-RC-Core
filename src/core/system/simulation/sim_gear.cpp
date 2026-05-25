/******************************************************************************
 * @file  sim_gear.cpp
 * @brief SimProc function — speed-based virtual gear FSM (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Sets `value = gear` — written to outCh (= GEAR) by the channel mechanism.
 *   Sub-gear button handling is now in `sim_subgear_btn_fn` (separate proc).
 *   Per-gear ramp time is written to `state->rampDynCfg` (RAM) when linked.
 *****************************************************************************/

#include "sim_gear.h"

#include "core/system/combus/combus_res.h"    // CbusNeutral

#include <Arduino.h>   // millis(), constrain()


// =============================================================================
// 1. FSM PRIMITIVES
// =============================================================================

/** @brief Reset a `GearFsmState` to gear 1, clear shift guard and sub-gear. */
void sim_gear_fsm_init(GearFsmState* state)
{
    state->gear           = 1;
    state->prevRpm        = 0;
    state->lastShiftMs    = 0u;
    state->subGear        = 0;
    state->prevSubGearSet = false;
    state->prevSubGearUp  = false;
    state->prevSubGearDn  = false;
}

/**
 * @brief Advance the N-gear FSM by one cycle — see sim_gear.h for contract.
 */
int8_t sim_gear_fsm_update(GearFsmState*           state,
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


// =============================================================================
// 2. SIMPROC FUNCTION
// =============================================================================

/** @brief Gear FSM side-effect proc — see sim_gear.h for full contract. */
void sim_gear_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg* cfg   = static_cast<const GearProcCfg*>(proc->cfg);
    GearFsmState*      state = static_cast<GearFsmState*>(proc->state);

    // --- 0. Self-init on first call (gear == 0 = zero-initialised state) -----
    if (state->gear == 0) {
        sim_gear_fsm_init(state);
    }

    // --- 1. RPM magnitude from RPM_BUS + DRIVE_STATE_BUS gate ---------------
    //    secInCh[0] = DRIVE_STATE_BUS (analog). Runner pre-reads into secInValue[0].
    //    secInCh[1] = SUBGEAR_BUS (analog). Runner pre-reads into secInValue[1].
    const int8_t  ds  = DriveStateBus::decode(proc->secInValue[0]);
    const int16_t rpm = (ds > 0) ? static_cast<int16_t>(value) : int16_t(0);

    // --- 2. Run gear FSM ------------------------------------------------------
    int8_t gear = sim_gear_fsm_update(state, *cfg->profile, rpm);

    // --- 3. Sub-gear lock (handled by sim_subgear_btn_fn upstream) ----------
    //    Lock gear at 1 while sub-gear is active.
    const int8_t subGear = static_cast<int8_t>(proc->secInValue[1]);
    if (subGear > 0 && gear > 1)
        gear = 1;

    // --- 4. Output — gear integer becomes the channel value -----------------
    value = static_cast<uint16_t>(gear);
}


// =============================================================================
// 3. APPLY-RATIO SIMPROC FUNCTION
// =============================================================================

/** @brief Shift-delta proc — subtracts shiftDelta RPM on upshift. */
void sim_apply_ratio_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    // secInCh[0] = DIRECT_DRIVE (digital). secInCh[1] = GEAR (analog).
    if (proc->secInValue[0] != 0u)
        return;  // Direct drive — skip RPM dip simulation.

    const GearProcCfg* cfg   = static_cast<const GearProcCfg*>(proc->cfg);
    ShiftDeltaState*   state = static_cast<ShiftDeltaState*>(proc->state);

    const int8_t curGear = static_cast<int8_t>(proc->secInValue[1]);

    // Upshift detected: subtract the entry delta of the new gear.
    if (curGear > state->prevGear && curGear >= 2) {
        const int16_t delta  = cfg->profile->gear[curGear - 1].shiftDelta;
        const int32_t result = static_cast<int32_t>(value) - static_cast<int32_t>(delta);
        value = static_cast<uint16_t>(result < 0 ? 0 : result);
    }

    state->prevGear = curGear;
}


// =============================================================================
// 4. RPM → ESC SPEED SIMPROC FUNCTION
// =============================================================================

/** @brief Gear direct-drive bypass — see sim_gear.h for contract. */
void sim_gear_bypass_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner /*chainOwner*/)
{
    // secInCh[0] = DIRECT_DRIVE (digital). Runner pre-reads into secInValue[0].
    if (proc->secInValue[0] != 0u) {
        value   = 1u;
        claimed = true;
    }
}


// =============================================================================
// 5. RPM → ESC SPEED SIMPROC FUNCTION
// =============================================================================

/** @brief RPM → ESC speed proc — see sim_gear.h for full contract. */
void sim_rpm_to_speed_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    // secInCh[0] = DRIVE_STATE_BUS (analog). secInCh[1] = GEAR (analog).
    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    const uint8_t           nGears  = profile->gearCount;

    // --- Direction from secInValue[0] (DRIVE_STATE_BUS) ----------------------
    const int8_t ds = DriveStateBus::decode(proc->secInValue[0]);

    // --- Resolve current gear from secInValue[1] (GEAR) ----------------------
    const int8_t rawGear = static_cast<int8_t>(proc->secInValue[1]);

    // --- Gear-accumulation formula -------------------------------------------
    uint16_t maxAbsRpm = profile->gear[nGears - 1u].upShift;
    for (uint8_t i = 1u; i < nGears; ++i)
        maxAbsRpm += profile->gear[i].shiftDelta;

    const uint8_t gearIdx = static_cast<uint8_t>(
        constrain(static_cast<int>(rawGear) - 1, 0, static_cast<int>(nGears) - 1));

    uint16_t cumDelta = 0u;
    for (uint8_t i = 1u; i <= gearIdx; ++i)
        cumDelta += profile->gear[i].shiftDelta;

    const uint32_t rpm       = static_cast<uint32_t>(value);
    const uint32_t numerator = (rpm + static_cast<uint32_t>(cumDelta))
                             * static_cast<uint32_t>(CbusNeutral);
    const uint16_t half      = (maxAbsRpm > 0u)
                             ? static_cast<uint16_t>(numerator / static_cast<uint32_t>(maxAbsRpm))
                             : uint16_t(0);

    if      (ds > 0) value = static_cast<uint16_t>(constrain(
                                 static_cast<int32_t>(CbusNeutral) + static_cast<int32_t>(half),
                                 0, static_cast<int32_t>(CbusMaxVal)));
    else if (ds < 0) value = static_cast<uint16_t>(constrain(
                                 static_cast<int32_t>(CbusNeutral) - static_cast<int32_t>(half),
                                 0, static_cast<int32_t>(CbusMaxVal)));
    else             value = CbusNeutral;
}


// =============================================================================
// 6. GEAR→RAMP BRIDGE SIMPROC FUNCTION
// =============================================================================

/** @brief Gear→ramp bridge — see sim_gear.h for contract. */
void sim_gear_ramp_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    if (proc->dynCfg == nullptr) return;  // No ramp linked — passthrough.

    const GearProcCfg* cfg = static_cast<const GearProcCfg*>(proc->cfg);
    SimRampCfg*        dyn = static_cast<SimRampCfg*>(proc->dynCfg);

    // --- Resolve gear and sub-gear ------------------------------------------
    //  secInCh[0] = SUBGEAR_BUS (analog). Runner pre-reads into secInValue[0].
    const int8_t  gear       = static_cast<int8_t>(value);
    const uint8_t subGearIdx = static_cast<uint8_t>(proc->secInValue[0]);

    // --- Look up new ramp time ----------------------------------------------
    uint16_t newRampTime;
    if (subGearIdx > 0u && cfg->profile->subGear != nullptr)
    {
        const uint8_t si = static_cast<uint8_t>(
            constrain(static_cast<int>(subGearIdx) - 1,
                      0, static_cast<int>(cfg->profile->subGearCount) - 1));
        newRampTime = cfg->profile->subGear[si].rampTime;
    }
    else
    {
        const uint8_t gi = static_cast<uint8_t>(
            constrain(static_cast<int>(gear) - 1,
                      0, static_cast<int>(cfg->profile->gearCount) - 1));
        newRampTime = cfg->profile->gear[gi].rampTime;
    }

    // --- Write dynCfg only on change ----------------------------------------
    if (dyn->rampTimeMs != newRampTime) {
        dyn->rampTimeMs = newRampTime;
        dyn->resetRamp  = true;
    }
    // value (= gear) passed through unchanged.
}

// EOF sim_gear.cpp
