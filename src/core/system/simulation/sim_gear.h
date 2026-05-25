/******************************************************************************
 * @file  sim_gear.h
 * @brief CbProc function — speed-based virtual gear FSM.
 *
 * @details Two-level public API:
 *
 *   1. FSM primitives (`sim_gear_fsm_init`, `sim_gear_fsm_update`):
 *      Pure RPM → gear logic, no ComBus involved.  Useful for explicit reset
 *      (e.g. on runlevel transition) or direct use outside a CbProc.
 *
 *   2. CbProc function (`sim_gear_fn`):
 *      Output processor — reads `value` to derive RPM, runs the FSM,
 *      sets `value = gear` (written to outCh = `GEAR` by the channel mechanism),
 *      and writes per-gear ramp time to `TRACTION_RAMP_BUS` and sub-gear index
 *      to `SUBGEAR_BUS` as side effects.  Does NOT set `claimed`.
 *      Intended as the sole proc in a dedicated SIM_GEAR channel (outCh = GEAR).
 *
 *   3. CbProc function (`sim_apply_ratio_fn`):
 *      Shift-delta processor — will apply accumulated `shiftDelta` RPM drops
 *      to the throttle signal on each upshift.  Currently a no-op passthrough
 *      pending implementation.
 *
 *      Assign to `CbProc::fn`.  Pair with `GearProcCfg` in `CbProc::cfg`
 *      and `GearFsmState` in `CbProc::state`.  Self-inits on first call
 *      when `state->gear == 0` (zero-init sentinel).
 *
 *      Typical placement: last proc in the chain — other procs (bypass, ramp)
 *      already handle the `value` pipeline; gear is a pure side channel.
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // CbProc, GearProcCfg, ShiftDeltaState, GearFsmState, GearShiftProfile


// =============================================================================
// 1. FSM PRIMITIVES
// =============================================================================

/**
 * @brief Reset a `GearFsmState` to gear 1, clearing shift guard and sub-gear.
 *
 * @details Called automatically by `sim_gear_fn` on first call when
 *   `state->gear == 0` (zero-init detection).  Exposed here for callers that
 *   need to force-reset a running FSM (e.g. on runlevel transition).
 *
 * @param state  Non-null pointer to the FSM state to reset.
 */
void sim_gear_fsm_init(GearFsmState* state);

/**
 * @brief Advance the N-gear FSM by one cycle.
 *
 * @details Compares `rpm` to the shift thresholds in `profile`:
 *   - Rising RPM trend above `gear[n].upShift`  → upshift (guarded).
 *   - Falling RPM below `gear[n].downShiftBraking` (or `gear[n].downShift`
 *     when not decelerating) → downshift (guarded).
 *   - `rpm <= 0` forces gear 1 immediately (reverse or standing).
 *
 * @param state    Mutable FSM state (gear, prevRpm, lastShiftMs).
 * @param profile  Read-only shift thresholds and guard timing.
 * @param rpm      Current RPM (positive = forward, ≤ 0 = standing/reverse).
 * @return         Active gear after this update (1–`profile.gears`).
 */
int8_t sim_gear_fsm_update(GearFsmState*           state,
                           const GearShiftProfile& profile,
                           int16_t                 rpm);


// =============================================================================
// 2. SIMPROC FUNCTION
// =============================================================================

/**
 * @brief Gear FSM CbProc — reads RPM magnitude, runs the FSM, sets `value = gear`.
 *
 * @details secInCh[0] = DRIVE_STATE_BUS (analog): gates RPM for reverse/standing.
 *          secInCh[1] = SUBGEAR_BUS (analog): suppresses upshift when active.
 *
 *   Sub-gear button handling is now in `sim_subgear_btn_fn` (separate proc,
 *   placed BEFORE this one in the gear channel).  This proc only reads the
 *   current sub-gear index to suppress upshifts.
 *
 *   Does NOT set `claimed`.
 *   cfg = GearProcCfg*, state = GearFsmState*.
 *
 * @param proc    CbProc descriptor.
 * @param value   In: RPM magnitude [0..maxRpm].  Out: active gear (1..N).
 * @param claimed Not modified.
 */
void sim_gear_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Gear direct-drive bypass — sets GEAR = 1 and claims when DIRECT_DRIVE is HIGH.
 *
 * @details secInCh[0] = DIRECT_DRIVE (digital).
 *   cfg = nullptr, state = nullptr.
 *
 * @param proc    CbProc descriptor.
 * @param value   Set to 1 when secInValue[0] != 0; unchanged otherwise.
 * @param claimed Set to `true` when DIRECT_DRIVE is HIGH; unchanged otherwise.
 */
void sim_gear_bypass_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Gear shift-delta CbProc — subtracts shiftDelta RPM on upshift.
 *
 * @details secInCh[0] = DIRECT_DRIVE (digital): early exit when HIGH.
 *          secInCh[1] = GEAR (analog): current gear from SIM_GEAR channel.
 *   cfg = GearProcCfg*, state = ShiftDeltaState*.
 *
 * @param proc    CbProc descriptor.
 * @param value   In: RPM magnitude.  Out: RPM after shift dip.
 * @param claimed Not modified.
 */
void sim_apply_ratio_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief RPM → ESC speed CbProc — converts RPM_BUS to ESC_SPEED_BUS domain.
 *
 * @details secInCh[0] = DRIVE_STATE_BUS (analog): direction encoding.
 *          secInCh[1] = GEAR (analog): active gear from SIM_GEAR channel.
 *   cfg = GearProcCfg*, state = nullptr.
 *
 * @param proc    CbProc descriptor.
 * @param value   In: RPM_BUS magnitude.  Out: ESC_SPEED_BUS bipolar value.
 * @param claimed Not modified.
 */
void sim_rpm_to_speed_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Gear→ramp bridge — updates per-gear ramp time in a linked SimRampCfg.
 *
 * @details secInCh[0] = SUBGEAR_BUS (analog): selects sub-gear ramp when active.
 *   Passes `value` through unchanged (gear flows to sim_write).
 *   cfg = GearProcCfg*, dynCfg = SimRampCfg* (RAM), state = nullptr.
 *
 * @param proc    CbProc descriptor.
 * @param value   In/out: current gear — passed through unchanged.
 * @param claimed Not modified.
 */
void sim_gear_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF sim_gear.h
