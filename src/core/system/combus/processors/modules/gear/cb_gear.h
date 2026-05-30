/******************************************************************************
 * @file  cb_gear.h
 * @brief Virtual gearbox CbProc wrappers.
 *
 * @details Six CbProc functions wrapping the gear FSM + transmission model:
 *
 *   1. `gear_fsm_fn` — FSM wrapper (RPM → gear).
 *      Output processor — reads `value` to derive RPM, runs the FSM,
 *      sets `value = gear` (written to outCh = `GEAR`).  Does NOT set `claimed`.
 *      inCh = DRIVE_STATE_BUS (analog): gates RPM for reverse/standing.
 *      cfg = GearProcCfg*, state = GearFsmState*.
 *
 *   2. `gear_upshift_drop_fn` — shift-delta processor.
 *      Subtracts shiftDelta RPM on upshift (simulates engine drop on ratio change).
 *      inCh = GEAR (analog): current gear from GEAR channel.
 *      cfg = GearProcCfg*, state = ShiftDeltaState*.
 *
 *   3. `gear_ratio_fn` — RPM → gear-adjusted magnitude (RPM domain).
 *      Adds cumulative shiftDelta of current gear to RPM magnitude.
 *      GEAR=0 → passthrough (neutral / idle sentinel).
 *      inCh = GEAR (analog): active gear from GEAR channel.
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   4. `gear_subgear_cap_fn` — sub-gear speed cap (RPM domain).
 *      Caps magnitude to maxSpeedPct when sub-gear is active.
 *      Passthrough when SUBGEAR_BUS == 0 (normal mode).
 *      inCh = SUBGEAR_BUS (analog): active sub-gear index (0 = inactive).
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   5. `gear_dir_fn` — unsigned RPM magnitude → bipolar ComBus.
 *      Applies direction (DRIVE_STATE_BUS) and scales to ComBus half range.
 *      value == 0 → CbusNeutral regardless of direction.
 *      inCh = DRIVE_STATE_BUS (analog): direction encoding.
 *      cfg = GearProcCfg*, state = nullptr.
 *
 *   6. `gear_dyn_ramp_fn` — gear → rampTime bridge.
 *      Updates per-gear ramp time in a linked CbRampCfg (dynCfg).
 *      No inCh — reads gear from `value` (= gear after claim cascade).
 *      cfg = GearProcCfg*, dynCfg = CbRampCfg* (RAM), state = nullptr.
 *      Intended placement: LAST proc in GEAR chain.
 *
 *   Pipeline pattern for TRACTION chain:
 *      gear_ratio_fn  →  gear_subgear_cap_fn  →  gear_dir_fn
 *      (magnitude domain throughout; direction applied once at the end).
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
 * @brief RPM → gear-adjusted magnitude — adds cumulative shiftDelta for current gear.
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

// EOF cb_gear.h

