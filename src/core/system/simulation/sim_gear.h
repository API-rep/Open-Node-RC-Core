/******************************************************************************
 * @file  sim_gear.h
 * @brief SimProc function — speed-based virtual gear FSM.
 *
 * @details Two-level public API:
 *
 *   1. FSM primitives (`sim_gear_fsm_init`, `sim_gear_fsm_update`):
 *      Pure RPM → gear logic, no ComBus involved.  Useful for explicit reset
 *      (e.g. on runlevel transition) or direct use outside a SimProc.
 *
 *   2. SimProc function (`sim_gear_fn`):
 *      Output processor — reads `value` to derive RPM, runs the FSM,
 *      sets `value = gear` (written to outCh = `GEAR` by the channel mechanism),
 *      and writes per-gear ramp time to `TRACTION_RAMP_BUS` and sub-gear index
 *      to `SUBGEAR_BUS` as side effects.  Does NOT set `claimed`.
 *      Intended as the sole proc in a dedicated SIM_GEAR channel (outCh = GEAR).
 *
 *   3. SimProc function (`sim_apply_ratio_fn`):
 *      Shift-delta processor — will apply accumulated `shiftDelta` RPM drops
 *      to the throttle signal on each upshift.  Currently a no-op passthrough
 *      pending implementation.
 *
 *      Assign to `SimProc::fn`.  Pair with `GearProcCfg` in `SimProc::cfg`
 *      and `GearFsmState` in `SimProc::state`.  Self-inits on first call
 *      when `state->gear == 0` (zero-init sentinel).
 *
 *      Typical placement: last proc in the chain — other procs (bypass, ramp)
 *      already handle the `value` pipeline; gear is a pure side channel.
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // SimProc, GearProcCfg, ShiftDeltaState, GearFsmState, GearShiftProfile
#include <struct/combus_struct.h>       // ComBus


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
 * @brief Gear FSM SimProc — reads RPM_BUS magnitude, runs the FSM, sets `value = gear`.
 *
 * @details Matches the `SimProcFn` signature.  Self-inits on first call when
 *   `state->gear == 0` (zero-init sentinel).
 *
 *   Actions per cycle:
 *   - Reads `value` (= RPM_BUS magnitude, seeded by SimChannel from inCh).
 *   - Gates RPM to 0 when `DRIVE_STATE_BUS` indicates reverse or standing
 *     (forces gear 1 — no conversion from ComBus position needed).
 *   - Runs the N-gear FSM (upshift / downshift / sub-gear toggle and step).
 *   - Sets `value = gear` — written to outCh (= GEAR) by the channel mechanism.
 *   - Writes per-gear ramp time (ms) to `TRACTION_RAMP_BUS` (side effect).
 *   - Writes sub-gear index (0 = inactive) to `SUBGEAR_BUS` (side effect).
 *
 *   Does NOT set `claimed` — not applicable when used as sole proc.
 *
 * @param proc    SimProc descriptor — `cfg` cast to `GearProcCfg*`,
 *                `state` cast to `GearFsmState*`.  Neither may be nullptr.
 * @param value   In: RPM magnitude from RPM_BUS [0..maxRpm].  Out: active gear (1..N).
 * @param bus     Read for DRIVE_STATE_BUS + SUBGEAR_* digital channels;
 *                written for TRACTION_RAMP_BUS / SUBGEAR_BUS.
 * @param claimed Not modified.
 */
void sim_gear_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

/**
 * @brief Gear direct-drive bypass — sets GEAR = 1 and claims when DIRECT_DRIVE is HIGH.
 *
 * @details Placed as the first proc in the SIM_GEAR pipeline.  When
 *   `DIRECT_DRIVE` is HIGH, writes `value = 1` (locks gear to 1, no upshifting)
 *   and sets `claimed = true` to skip `sim_gear_fn`.
 *
 *   GEAR = 1 flows into `sim_rpm_to_speed_fn` normally — gear-accumulation
 *   formula applies with cumDelta = 0 (same as gear 1 in simulation mode).
 *
 *   No cfg or state needed: `SimProc::cfg` and `SimProc::state` must be `nullptr`.
 *
 * @param proc    Unused (cfg and state are nullptr).
 * @param value   Set to 1 when DIRECT_DRIVE is HIGH; unchanged otherwise.
 * @param bus     Read for DIRECT_DRIVE digital channel.
 * @param claimed Set to `true` when DIRECT_DRIVE is HIGH; unchanged otherwise.
 */
void sim_gear_bypass_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

/**
 * @brief Gear shift-delta SimProc — subtracts `shiftDelta` RPM on upshift.
 *
 * @details Applied as 4th proc on SIM_THROTTLE, after `sim_scale_fn`, so
 *   `value` is the RPM magnitude [0..maxRpm] at entry.
 *
 *   Early exit when `DIRECT_DRIVE` digital channel is HIGH (inertia bypassed).
 *
 *   On upshift detection (curGear > prevGear):
 *     `value = max(0, value − profile->gear[curGear−1].shiftDelta)`
 *   The result is the RPM after the engine dip on gear change.
 *   The inertia ramp in `SIM_TRACTION` will smooth the dip naturally.
 *
 *   cfg = `GearProcCfg*` (shared with sim_gear_fn if using the same profile).
 *   state = `ShiftDeltaState*` (prevGear only — separate from GearFsmState).
 *
 *   Reads `GEAR` from bus (previous tick — 1-tick lag acceptable at 20 ms).
 *   Does NOT set `claimed`.
 *
 * @param proc    SimProc descriptor — `cfg` cast to `GearProcCfg*`,
 *                `state` cast to `ShiftDeltaState*`.
 * @param value   In: RPM magnitude [0..maxRpm].  Out: RPM after shift dip.
 * @param bus     Read for DIRECT_DRIVE (early exit) and GEAR.
 * @param claimed Not modified.
 */
void sim_apply_ratio_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

/**
 * @brief RPM → ESC speed SimProc — converts RPM_BUS to ESC_SPEED_BUS domain.
 *
 * @details Applied as the sole proc in SIM_TRACTION (inCh = RPM_BUS,
 *   outCh = ESC_SPEED_BUS).
 *
 *   Gear-accumulation formula for all cases (DIRECT_DRIVE or not):
 *
 *   **DIRECT_DRIVE HIGH** — `sim_gear_bypass_fn` locked GEAR = 1; cumDelta = 0.
 *   Same formula applies, inertia/shiftDelta were skipped upstream.
 *
 *   **DIRECT_DRIVE LOW** — GEAR reflects the active gear from `sim_gear_fn`.
 *   Same formula applies with per-gear cumDelta.
 *
 *   Gear-accumulation formula:
 *   @code
 *     maxAbsRpm   = upShift[topGear] + sum(shiftDelta[1..N])
 *     cumDelta[g] = sum(shiftDelta[1..g])   (0 for gear 1)
 *     speed_half  = (rpm + cumDelta[gear]) * CbusNeutral / maxAbsRpm
 *   @endcode
 *   Direction from DRIVE_STATE_BUS in both modes.
 *
 *   Stateless: `SimProc::state` must be `nullptr`.
 *   cfg = `GearProcCfg*` (shared with sim_gear_fn and sim_apply_ratio_fn).
 *
 * @param proc    SimProc descriptor — `cfg` cast to `const GearProcCfg*`.
 *                `state` is unused (must be nullptr).
 * @param value   In: RPM_BUS value.  Out: ESC_SPEED_BUS bipolar value.
 * @param bus     Read for DIRECT_DRIVE, GEAR, DRIVE_STATE_BUS.
 * @param claimed Not modified.
 */
void sim_rpm_to_speed_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

// EOF sim_gear.h
