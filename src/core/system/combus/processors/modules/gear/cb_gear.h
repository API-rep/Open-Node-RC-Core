/******************************************************************************
 * @file  cb_gear.h
 * @brief Virtual gearbox CbProc wrappers.
 *
 * @details Seven CbProc functions wrapping the gear FSM + transmission model:
 *
 *   1. `gear_fsm_fn` -- FSM wrapper (RPM to gear).
 *      Output processor -- reads `value` to derive RPM, runs the FSM,
 *      sets `value = gear` (written to outCh = `GEAR`).  Does NOT set `claimed`.
 *      inCh = DRIVE_STATE_BUS (analog): gates RPM for reverse/standing.
 *      cfg = GearProcCfg*, state = GearFsmState*.
 *
 *   2. `gear_ratio_inv_fn` -- wheel_speed x 1000 / gearRatio[prevGear] = engine_rpm.
 *      Wheel-speed-primary inverse transform: converts wheel_speed to engine RPM.
 *      Placed FIRST in GEAR chain (before gear_fsm_fn).  Gear 0 -> passthrough.
 *      Writes engine_rpm to proc->outValue (committed to outCh = ESC_RPM_BUS).
 *      Passes engine_rpm downstream in `value` for gear_fsm_fn.
 *      state = GearFsmState* (read-only; shared with gear_fsm_fn).
 *      cfg = GearProcCfg*.
 *
 *   3. `gear_ratio_fn` -- RPM x gearRatio[gear]/1000 -> wheel-speed RPM (RPM domain).
 *      Forward multiplicative transform (RPM-primary mode, kept for reference).
 *      GEAR=0 -> passthrough (neutral / idle sentinel).
 *      inCh = GEAR (analog): active gear from GEAR channel.
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   4. `gear_subgear_cap_fn` -- sub-gear speed cap (RPM domain).
 *      Caps magnitude to maxSpeedPct when sub-gear is active.
 *      Passthrough when SUBGEAR_BUS == 0 (normal mode).
 *      inCh = SUBGEAR_BUS (analog): active sub-gear index (0 = inactive).
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   5. `gear_dir_fn` -- unsigned RPM magnitude to bipolar ComBus.
 *      Applies direction (DRIVE_STATE_BUS) and scales to ComBus half range.
 *      value == 0 -> CbusNeutral regardless of direction.
 *      inCh = DRIVE_STATE_BUS (analog): direction encoding.
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   6. `gear_dyn_ramp_fn` -- gear to rampTime bridge.
 *      Updates per-gear ramp time in a linked CbRampCfg (dynCfg).
 *      No inCh -- reads gear from `value` (= gear after claim cascade).
 *      cfg = GearProcCfg*, dynCfg = CbRampCfg* (RAM), state = nullptr.
 *      Intended placement: LAST proc in GEAR chain.
 *
 *   7. `gear_upshift_damp_fn` -- detect upshift; damp traction accel for a timed window.
 *      Observer in TRACTION chain -- does NOT modify `value` (wheel_speed passes through).
 *      Reads current gear via inCh = GEAR (analog).
 *      On upshift: writes profile->upshiftDampSteps to dynCfg->extAccelSteps for
 *      profile->upshiftDampMs.  Clears extAccelSteps when window expires.
 *      cfg = GearProcCfg*, dynCfg = CbRampCfg* (traction ramp, RAM),
 *      state = GearDampState*.
 *
 *   Pipeline pattern for GEAR chain (wheel-speed-primary mode):
 *      gear_ratio_inv_fn  ->  gear_fsm_fn  ->  gear_dyn_ramp_fn
 *      (wheel_speed in; engine_rpm -> ESC_RPM_BUS side-effect; gear -> GEAR out).
 *
 *   Pipeline pattern for TRACTION chain:
 *      upshift-damp  ->  gear_subgear_cap_fn  ->  gear_dir_fn
 *      (wheel_speed magnitude domain; direction applied once at the end).
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                         // CbProc, ChanOwner
#include <struct/combus/processors/modules/gear_struct.h>      // GearProcCfg, GearFsmState


/**
 * @brief Gear FSM CbProc — reads RPM magnitude, runs the FSM, sets `value = gear`.
 */
void gear_fsm_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Wheel-speed-primary inverse transform: wheel_speed / gearRatio = engine_rpm.
 *
 * @details Placed FIRST in GEAR chain.  Reads wheel_speed from `value`.
 *   Writes engine_rpm to proc->outValue (committed to outCh = ESC_RPM_BUS).
 *   Passes engine_rpm downstream in `value` for gear_fsm_fn.
 *   Gear 0 (uninitialised) -> passthrough.
 *   state = GearFsmState* (read-only, shared with gear_fsm_fn).
 */
void gear_ratio_inv_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief RPM × gearRatio[gear]/1000 → wheel-speed RPM (RPM domain).
 *
 * @details GEAR=0 → passthrough.  Result stays in RPM domain for `gear_dir_fn`.
 *   inCh = GEAR (analog).
 */
void gear_ratio_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Sub-gear speed cap — caps magnitude to maxSpeedPct when SUBGEAR active.
 *
 * @details Passthrough when SUBGEAR_BUS == 0.  Operates in RPM domain.
 *   inCh = SUBGEAR_BUS (analog).
 */
void gear_subgear_cap_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Apply direction — converts unsigned RPM magnitude to bipolar ComBus.
 *
 * @details Scales `value` against maxAbsRpm, then offsets by CbusNeutral.
 *   value == 0 → CbusNeutral.
 *   inCh = DRIVE_STATE_BUS (analog).
 */
void gear_dir_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Gear → ramp bridge — updates per-gear ramp time in a linked CbRampCfg.
 */
void gear_dyn_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Observer: detect upshift, damp traction accel ramp for a timed window.
 *
 * @details Placed in TRACTION chain (after `in` proc).  Does NOT modify `value`.
 *   Reads current gear via proc->inValue (inCh = GEAR).
 *   On upshift: writes profile->upshiftDampSteps to dynCfg->extAccelSteps and
 *   arms a timer for profile->upshiftDampMs.  Clears extAccelSteps on expiry.
 *   dynCfg = CbRampCfg* (traction ramp, mutable),  state = GearDampState*.
 */
void gear_upshift_damp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_gear.h

