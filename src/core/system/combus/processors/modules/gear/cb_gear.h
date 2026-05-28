/******************************************************************************
 * @file  cb_gear.h
 * @brief Virtual gearbox CbProc wrappers.
 *
 * @details Four CbProc functions wrapping the gear FSM + transmission model:
 *
 *   1. `gear_fsm_fn` — FSM wrapper (RPM → gear).
 *      Output processor — reads `value` to derive RPM, runs the FSM,
 *      sets `value = gear` (written to outCh = `GEAR`).  Does NOT set `claimed`.
 *      inCh[0] = DRIVE_STATE_BUS (analog): gates RPM for reverse/standing.
 *      cfg = GearProcCfg*, state = GearFsmState*.
 *
 *   2. `gear_upshift_drop_fn` — shift-delta processor.
 *      Subtracts shiftDelta RPM on upshift (simulates engine drop on ratio change).
 *      inCh[0] = GEAR (analog): current gear from GEAR channel.
 *      cfg = GearProcCfg*, state = ShiftDeltaState*.
 *
 *   3. `gear_rpm_to_speed_fn` — RPM → ESC speed conversion.
 *      Converts RPM_BUS to ESC_SPEED_BUS domain (cumulative deltas + direction).
 *      inCh[0] = DRIVE_STATE_BUS (analog): direction encoding.
 *      inCh[1] = GEAR (analog): active gear from GEAR channel.
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   4. `gear_dyn_ramp_fn` — gear → rampTime bridge.
 *      Updates per-gear ramp time in a linked CbRampCfg (dynCfg).
 *      No inCh — reads gear from `value` (= gear after claim cascade).
 *      cfg = GearProcCfg*, dynCfg = CbRampCfg* (RAM), state = nullptr.
 *      Intended placement: LAST proc in GEAR chain.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                         // CbProc, ChanOwner
#include <struct/combus/processors/modules/gear_struct.h>      // GearProcCfg, GearFsmState, ShiftDeltaState


/**
 * @brief Gear FSM CbProc — reads RPM magnitude, runs the FSM, sets `value = gear`.
 */
void gear_fsm_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Gear shift-delta CbProc — subtracts shiftDelta RPM on upshift.
 */
void gear_upshift_drop_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief RPM → ESC speed CbProc — converts RPM_BUS to ESC_SPEED_BUS domain.
 */
void gear_rpm_to_speed_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Gear → ramp bridge — updates per-gear ramp time in a linked CbRampCfg.
 */
void gear_dyn_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Sub-gear RPM cap — clamps RPM magnitude when sub-gear mode is active.
 *
 * @details Reads SUBGEAR_BUS index from inCh[0].  If non-zero, caps `value` to
 *   `profile->gear[0].upShift × subGear[idx-1].maxPct / 100`.
 *   Passthrough when sub-gear mode is inactive (SUBGEAR_BUS == 0) or when the
 *   profile has no sub-gear steps.
 *
 *   Intended placement: THROTTLE chain, immediately after the `scale` proc.
 *   inCh[0] = SUBGEAR_BUS (analog).
 *   cfg = GearProcCfg*, state = nullptr.
 */
void gear_subgear_rpm_cap_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_gear.h
