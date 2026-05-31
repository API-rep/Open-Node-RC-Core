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
 *   7. `gear_upshift_damp_fn` -- detect upshift; freeze traction accel + signal GEAR_SHIFTING.
 *      Placed LAST in GEAR chain (after gear_dyn_ramp_fn, before `out`).
 *      Reads current gear directly from `value` (no inCh needed).
 *      On upshift: sets dynCfg->extAccelSteps = INT16_MIN — effective accel step
 *      = max(1, accelSteps + INT16_MIN) ≈ 1 unit/tick, negligible over the window.
 *      Only the acceleration direction is damped; extBrakeSteps is never touched
 *      so braking (L2) remains functional.  rampTimeMs is not modified.
 *      On expiry: resets extAccelSteps = 0 (restores normal acceleration).
 *      Sets proc->outValue = 1 while freeze active — runner commits to
 *      outCh = GEAR_SHIFTING (machine-local digital).
 *      cfg = GearProcCfg* (upshiftDampMs), dynCfg = CbRampCfg* (traction ramp, RAM),
 *      state = GearDampState*.
 *      Rationale for GEAR placement: disabling the gear chain disables the freeze
 *      automatically — no stale rampTimeMs override on the THROTTLE chain.
 *      @todo winter 2026: promote GEAR_SHIFTING to WIRE region.
 *
 *   8. `gear_upshift_rpm_fade_fn` -- smoothly interpolate ESC_RPM_BUS rpmAtShift -> natural RPM.
 *      Placed BETWEEN gear-inv-ratio and gear-fsm in GEAR chain.
 *      Every cycle: stores state->lastRpm = value (ref for rpmAtShift on next upshift).
 *      During damp window: linearly interpolates ESC_RPM_BUS from rpmAtShift to natural
 *      engine_rpm over upshiftDampMs; passes `value` through unchanged for gear_fsm_fn.
 *      Outside window: write-through (proc->outValue = value).
 *      outCh = ESC_RPM_BUS,  cfg = GearProcCfg*,  state = GearDampState* (shared with 7).
 *
 *   Pipeline pattern for GEAR chain (wheel-speed-primary mode):
 *      gear_ratio_inv_fn  ->  gear_upshift_rpm_fade_fn  ->  [claim cascade]  ->
 *      gear_fsm_fn  ->  gear_dyn_ramp_fn  ->  gear_upshift_damp_fn
 *      (wheel_speed in; engine_rpm -> ESC_RPM_BUS side-effect via fade fn;
 *       gear -> GEAR out; GEAR_SHIFTING digital written by gear_upshift_damp_fn).
 *
 *   Pipeline pattern for TRACTION chain:
 *      gear_subgear_cap_fn  ->  gear_dir_fn
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
 * @brief Detect upshift; freeze traction ramp for upshiftDampMs + signal GEAR_SHIFTING.
 *
 * @details Placed in GEAR chain (after gear_dyn_ramp_fn, before out).  Reads current gear
 *   from `value`.  Does NOT modify `value`.
 *   On upshift: sets dynCfg->rampTimeMs = UINT16_MAX — the ramp proc never ticks
 *   while this value is set, freezing RPM at the upshift point.
 *   gear_dyn_ramp_fn (runs before this proc) restores the correct rampTimeMs on
 *   the first cycle after expiry (detects UINT16_MAX ≠ gear rampTime → resetRamp).
 *   Does NOT touch extAccelSteps or extBrakeSteps.
 *   Sets proc->outValue = 1 while freeze active; runner commits to GEAR_SHIFTING digital.
 *   dynCfg = CbRampCfg* (traction ramp, mutable),  state = GearDampState*.
 */
void gear_upshift_damp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Progressive RPM interpolation on ESC_RPM_BUS during upshift damp window.
 *
 * @details Placed AFTER gear-inv-ratio, BEFORE gear-fsm in GEAR chain.
 *   Every cycle: stores `state->lastRpm = value` (natural engine_rpm from gear-inv-ratio).
 *   During damp window: linearly interpolates ESC_RPM_BUS from `state->rpmAtShift`
 *   (RPM at the upshift moment) to the current natural RPM over `profile->upshiftDampMs`.
 *   Outside window or when upshiftDampMs == 0: pass-through (proc->outValue = value).
 *   Does NOT modify `value` — gear_fsm_fn downstream sees true natural RPM.
 *   outCh = ESC_RPM_BUS,  cfg = GearProcCfg*,  state = GearDampState* (shared with
 *   gear_upshift_damp_fn; both procs must reference the same GearDampState instance).
 */
void gear_upshift_rpm_fade_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_gear.h

