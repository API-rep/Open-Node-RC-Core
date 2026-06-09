/******************************************************************************
 * @file  cb_cruise.h
 * @brief CbProc functions — throttle hold (cruise-control).
 *
 * @details `cb_cruise_fn()` holds the throttle value at a set point, with two modes:
 *
 *   **Normal mode** (`holdSpeed = false`):
 *     Activated via `state->active` — set each cycle by `cb_cruise_sync_fn` from
 *     `CRUISE_ACTIVE` ComBus channel (maintained by the INPUT cruise chain).
 *     Rising edge: latches `heldValue = value`.
 *     Floor hold: if value < held → clamp to held; if value > held → pass through + update held.
 *     Braking (`extBrakeSteps > 0`): deactivates + syncs ramp `currentPos` to heldValue.
 *
 *   **holdSpeed mode** (`holdSpeed = true`):
 *     Activated by `inCh` (SUBGEAR_BUS != 0) **OR** `state->active` (□ button via CRUISE_ACTIVE).
 *     Both activation sources share the same holdSpeed (adaptive watermark) behaviour:
 *       rising edge: latches `heldValue = value`.
 *       braking: follows speed downward; holds at braked-to speed (no deactivation).
 *       speed above held: tracks upward (new held = value).
 *       speed below held: floor hold (value = held).
 *
 *   **Update cruise speed** (both modes):
 *     Set `state->updateReq = true` — done each cycle by `cb_cruise_upd_fn` from
 *     `CRUISE_UPDATE_BTN` ComBus channel (L3 button). On next cruise proc cycle:
 *     `heldValue ← current value`.
 *
 *   **Placement in chain** (SIM throttle):
 *     5.3  `cb_cruise_sync_fn` — CRUISE_ACTIVE → `state->active` (passthrough).
 *     5.4  `cb_cruise_upd_fn`  — CRUISE_UPDATE_BTN → `state->updateReq` (passthrough).
 *     5.5  `cb_cruise_fn`      — holdSpeed via SUBGEAR_BUS / normal via `state->active`.
 *     Must come after `brake` (extBrakeSteps already set) and before `center`
 *     (value still in bipolar domain — ramp sync is direct).
 *
 *   cfg   = `const CbCruiseCfg*`  (ROM-resident: holdSpeed + ramp pointers).
 *   state = `CbCruiseState*`       (RAM: active, heldValue, wasActive, updateReq).
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                         // CbProc, ChanOwner
#include <struct/combus/processors/motion/cb_cruise_struct.h>  // CbCruiseCfg, CbCruiseState


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Sync normal-cruise activation from ComBus — assigned to `CbProc::fn`.
 *
 * @details Passthrough helper: reads `proc->inValue` (expected `inCh = CRUISE_ACTIVE`)
 *   and writes it to `state->active`.  Must run in the SIM throttle chain BEFORE
 *   `cb_cruise_fn` so that the cruise proc sees the updated activation flag.
 *
 *   cfg   = nullptr (unused).
 *   state = `CbCruiseState*`.
 */
void cb_cruise_sync_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Set cruise speed-update request from ComBus — assigned to `CbProc::fn`.
 *
 * @details Passthrough helper: when `proc->inValue != 0` (expected `inCh = CRUISE_UPDATE_BTN`,
 *   L3 button pressed), sets `state->updateReq = true`.  On the same cycle, `cb_cruise_fn`
 *   latches `heldValue = value`.  Must run BEFORE `cb_cruise_fn`.
 *
 *   cfg   = nullptr (unused).
 *   state = `CbCruiseState*`.
 */
void cb_cruise_upd_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

/**
 * @brief Throttle hold proc — assigned to `CbProc::fn`.
 *
 * @param proc       CbProc descriptor. `cfg` = const CbCruiseCfg*; `state` = CbCruiseState*.
 * @param value      In/out throttle value in bipolar chain domain (before center/abs/scale).
 * @param claimed    Not modified.
 * @param chainOwner Not used.
 */
void cb_cruise_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_cruise.h
