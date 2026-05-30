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

    // --- RPM magnitude from RPM_BUS + DRIVE_STATE_BUS gate -------------------
    //     inCh = DRIVE_STATE_BUS (analog). Runner pre-reads into inValue.
    const int8_t  ds  = DriveStateBus::decode(proc->inValue);
    const int16_t rpm = (ds > 0) ? static_cast<int16_t>(value) : int16_t(0);

    // --- Run gear FSM --------------------------------------------------------
    const int8_t gear = gear_fsm_update(state, *cfg->profile, rpm);

    // --- Output â€” gear integer becomes the channel value ---------------------
    value = static_cast<uint16_t>(gear);
}


// =============================================================================
// 2. SHIFT-DELTA PROCESSOR
// =============================================================================

void gear_upshift_drop_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const GearProcCfg* cfg   = static_cast<const GearProcCfg*>(proc->cfg);
    ShiftDeltaState*   state = static_cast<ShiftDeltaState*>(proc->state);

    // inCh = GEAR (analog).
    const int8_t curGear = static_cast<int8_t>(proc->inValue);

    // Upshift detected: subtract the entry delta of the new gear.
    if (curGear > state->prevGear && curGear >= 2) {
        const int16_t delta  = cfg->profile->gear[curGear - 1].shiftDelta;
        const int32_t result = static_cast<int32_t>(value) - static_cast<int32_t>(delta);
        value = static_cast<uint16_t>(result < 0 ? 0 : result);
    }

    state->prevGear = curGear;
}


// =============================================================================
// 3. RPM â†’ GEAR-ADJUSTED MAGNITUDE
// =============================================================================

/** @brief Add cumulative shiftDelta of current gear to RPM magnitude (RPM domain). */
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

    uint16_t cumDelta = 0u;
    for (uint8_t i = 1u; i <= gearIdx; ++i)
        cumDelta += profile->gear[i].shiftDelta;

    // Add cumulative shiftDelta â€” stays in RPM domain for gear_dir_fn.
    const uint32_t adjusted = static_cast<uint32_t>(value) + static_cast<uint32_t>(cumDelta);
    value = adjusted > 0xFFFFu ? uint16_t(0xFFFFu) : static_cast<uint16_t>(adjusted);
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

    // Compute maxAbsRpm â€” same denominator used by gear_dir_fn.
    uint16_t maxAbsRpm = profile->gear[profile->gearCount - 1u].upShift;
    for (uint8_t i = 1u; i < profile->gearCount; ++i)
        maxAbsRpm += profile->gear[i].shiftDelta;

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

    // Compute maxAbsRpm â€” denominator for the ComBus half scale.
    uint16_t maxAbsRpm = profile->gear[nGears - 1u].upShift;
    for (uint8_t i = 1u; i < nGears; ++i)
        maxAbsRpm += profile->gear[i].shiftDelta;

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
// 6. GEAR â†’ RAMP BRIDGE
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

// EOF cb_gear.cpp
