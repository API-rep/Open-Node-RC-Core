/******************************************************************************
 * @file  sim_math.h
 * @brief SimProc functions \u2014 generic arithmetic transforms. *
 * @details Stateless (or state-free) transforms composable in any SimChannel
 *   pipeline.  All functions match `CbProcFn` (= `SimProcFn`) — no bus parameter.
 *
 *   `sim_center_fn`      —  signed deviation from CbusNeutral, packed in uint16_t:
 *                              `value = (uint16_t)(int16_t)(value − CbusNeutral)`
 *                            Positive = FWD side, negative = REV side.
 *                            cfg = &SimCenterCfg, state = nullptr.
 *
 *   `sim_abs_fn`         —  absolute value of a signed-packed uint16_t:
 *                              `value = |reinterpret<int16_t>(value)|`
 *                            cfg = nullptr, state = nullptr.
 *                            Sign side effect (HIGH = FWD, LOW = REV) via:
 *                              `proc->secOutValue`  (0 or 1)
 *                              `proc->optSecOutCh`  (digital channel; nullopt = no effect)
 *                            Runner commits optSecOutCh → secOutValue after fn.
 *
 *   `sim_scale_fn`       —  linear domain rescale:
 *                              `value = value × outMax / inMax`
 *                            cfg = &SimScaleCfg, state = nullptr.
 *
 *   `sim_drive_state_fn` —  drive-state observer (value NOT modified):
 *                            Reads post-ramp bipolaire value, encodes direction
 *                            via DriveStateBus::encode(), writes to:
 *                              `proc->secOutValue`  (encoded DriveState)
 *                              `proc->optSecOutCh`  (analog, e.g. DRIVE_STATE_BUS)
 *                            Runner commits optSecOutCh → secOutValue after fn.
 *                            cfg = &SimDriveStateCfg, state = nullptr.
 *
 *   Typical three-proc chain (THROTTLE_BUS → RPM_BUS):
 *
 *     sim_center_fn  →  signed(throttle − CbusNeutral)  range [−500..+500] as int16
 *     sim_abs_fn     →  |signed|, optSecOutCh = FWD_FLAG  range [0..CbusNeutral]
 *     sim_scale_fn   →  magnitude × maxRpm / CbusNeutral    range [0..maxRpm]
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>  // SimProc, SimScaleCfg, SimProcFn


// =============================================================================
// 1. SIMPROC FUNCTIONS
// =============================================================================

/**
 * @brief Signed center SimProc — `value = (uint16_t)(int16_t)(value − cfg->neutral)`.
 *
 * @details cfg = &SimCenterCfg, state = nullptr.  Does NOT set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `const SimCenterCfg*`.
 * @param value   In: unsigned ComBus [0..CbusMaxVal].  Out: signed packed int16.
 * @param claimed Not modified.
 */
void sim_center_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner chanOwner);

/**
 * @brief Absolute-value SimProc — `value = |reinterpret<int16_t>(value)|`.
 *
 * @details Interprets `value` as two's complement int16_t (as produced by
 *   `sim_center_fn`), computes its absolute value, and writes it back.
 *   cfg = nullptr, state = nullptr.  Does NOT set `claimed`.
 *
 *   Sign side effect (direction flag) via:
 *   - `proc->secOutValue` = 1 (FWD) or 0 (REV)
 *   - `proc->optSecOutCh` = digital channel; runner commits after fn
 *     (nullopt = no side effect).
 *
 * @param proc    CbProc descriptor — `optSecOutCh` sets the sign side effect channel.
 * @param value   In: signed-packed int16.  Out: magnitude [0..CbusNeutral].
 * @param claimed Not modified.
 */
void sim_abs_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner chanOwner);

/**
 * @brief Linear scale SimProc — `value = value × outMax / inMax`.
 *
 * @details Rescales `value` from [0..inMax] to [0..outMax].  No clamping.
 *   cfg = &SimScaleCfg, state = nullptr.  Does NOT set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `const SimScaleCfg*`.
 * @param value   In/Out: value to rescale.
 * @param claimed Not modified.
 */
void sim_scale_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner chanOwner);

/**
 * @brief Drive-state observer SimProc — encodes direction and writes DRIVE_STATE_BUS.
 *
 * @details Reads post-ramp bipolaire `value`, compares to `cfg->neutral`:
 *   - value > neutral  → DriveState::kDriveFwd
 *   - value < neutral  → DriveState::kDriveRev
 *   - value == neutral → DriveState::kStanding
 *   Encodes result via DriveStateBus::encode() and stores it in:
 *   - `proc->secOutValue` (runner commits to proc->optSecOutCh)
 *   Does NOT modify `value` — pure side-effect observer.
 *   Does NOT set `claimed`.
 *   cfg = &SimDriveStateCfg, state = nullptr.
 *
 * @param proc    CbProc descriptor — `cfg` = SimDriveStateCfg; `optSecOutCh` = DRIVE_STATE_BUS.
 * @param value   Read-only — post-ramp bipolaire position.
 * @param claimed Not modified.
 */
void sim_drive_state_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner chanOwner);

// EOF sim_math.h
