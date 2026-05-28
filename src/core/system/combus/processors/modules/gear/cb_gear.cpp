/******************************************************************************
 * @file  cb_gear.cpp
 * @brief Virtual gearbox CbProc wrappers — implementation.
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

    // --- RPM magnitude from RPM_BUS + DRIVE_STATE_BUS gate -------------------
    //     inCh[0] = DRIVE_STATE_BUS (analog). Runner pre-reads into inValue[0].
    const int8_t  ds  = DriveStateBus::decode(proc->inValue[0]);
    const int16_t rpm = (ds > 0) ? static_cast<int16_t>(value) : int16_t(0);

    // --- Run gear FSM --------------------------------------------------------
    const int8_t gear = gear_fsm_update(state, *cfg->profile, rpm);

    // --- Output — gear integer becomes the channel value ---------------------
    value = static_cast<uint16_t>(gear);
}


// =============================================================================
// 2. SHIFT-DELTA PROCESSOR
// =============================================================================

void gear_upshift_drop_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg* cfg   = static_cast<const GearProcCfg*>(proc->cfg);
    ShiftDeltaState*   state = static_cast<ShiftDeltaState*>(proc->state);

    // inCh[0] = GEAR (analog).
    const int8_t curGear = static_cast<int8_t>(proc->inValue[0]);

    // Upshift detected: subtract the entry delta of the new gear.
    if (curGear > state->prevGear && curGear >= 2) {
        const int16_t delta  = cfg->profile->gear[curGear - 1].shiftDelta;
        const int32_t result = static_cast<int32_t>(value) - static_cast<int32_t>(delta);
        value = static_cast<uint16_t>(result < 0 ? 0 : result);
    }

    state->prevGear = curGear;
}


// =============================================================================
// 3. RPM → ESC SPEED CONVERTER
// =============================================================================

void gear_rpm_to_speed_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;
    const uint8_t           nGears  = profile->gearCount;

    // --- Direction from inValue[0] (DRIVE_STATE_BUS) ------------------------
    const int8_t ds = DriveStateBus::decode(proc->inValue[0]);

    // --- Resolve current gear from inValue[1] (GEAR) ------------------------
    const int8_t rawGear = static_cast<int8_t>(proc->inValue[1]);

    // GEAR = 0 sentinel: sub-gear mode active — sub-gear speed proc handles output.
    if (rawGear == 0) { value = CbusNeutral; return; }

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
// 4. GEAR → RAMP BRIDGE
// =============================================================================

void gear_dyn_ramp_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    if (proc->dynCfg == nullptr) return;  // No ramp linked — passthrough.

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
// 5. SUB-GEAR SPEED OUTPUT
// =============================================================================

void gear_subgear_speed_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    // inValue[0] = SUBGEAR_BUS (0 = inactive, 1..N = sub-gear index).
    const uint8_t subIdx = static_cast<uint8_t>(proc->inValue[0]);
    if (subIdx == 0u) return;  // Normal mode — passthrough, rpm_to_speed result stands.

    const GearProcCfg*      cfg     = static_cast<const GearProcCfg*>(proc->cfg);
    const GearShiftProfile* profile = cfg->profile;

    if (profile->subGear == nullptr || profile->subGearCount == 0u) {
        value = CbusNeutral;
        return;
    }

    // Clamp sub-gear index to valid range (1-based → 0-based).
    const uint8_t gi = static_cast<uint8_t>(
        constrain(static_cast<int>(subIdx) - 1, 0, static_cast<int>(profile->subGearCount) - 1));

    // Direction from inValue[1] = DRIVE_STATE_BUS.
    const int8_t ds = DriveStateBus::decode(proc->inValue[1]);

    // RPM magnitude from inValue[2] = RPM_BUS (re-read by runner, independent of value).
    const uint32_t rpm = static_cast<uint32_t>(proc->inValue[2]);

    // Max RPM = last gear upShift (same reference as kThrottleScale.outMax).
    const uint32_t maxRpm = static_cast<uint32_t>(profile->gear[profile->gearCount - 1u].upShift);

    // Speed half = rpm × (CbusNeutral × maxSpeedPct / 100) / maxRpm.
    const uint32_t maxHalf = static_cast<uint32_t>(CbusNeutral)
                           * static_cast<uint32_t>(profile->subGear[gi].maxSpeedPct) / 100u;
    const uint32_t half    = (maxRpm > 0u) ? (rpm * maxHalf / maxRpm) : 0u;

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

// EOF cb_gear.cpp
