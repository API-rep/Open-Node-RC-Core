/******************************************************************************
 * @file  sim_math.h
 * @brief SimProc functions \u2014 generic arithmetic transforms.
 *
 * @details Three stateless transforms composable in any SimChannel pipeline:
 *
 *   `sim_center_fn`  \u2014  signed deviation from CbusNeutral, packed in uint16_t:
 *                         `value = (uint16_t)(int16_t)(value \u2212 CbusNeutral)`
 *                       Positive = FWD side, negative = REV side (two's complement).
 *                       cfg = nullptr, state = nullptr.
 *
 *   `sim_abs_fn`     \u2014  absolute value of a signed-packed uint16_t:
 *                         `value = |reinterpret<int16_t>(value)|`
 *                       **cfg-free** (cfg = nullptr, state = nullptr).
 *                       Optional digital side effect via `proc->optOutCh` (digital):
 *                         HIGH = positive (FWD), LOW = negative (REV).
 *                         std::nullopt = no side effect.
 *
 *   `sim_scale_fn`   \u2014  linear domain rescale:
 *                         `value = value \u00d7 outMax / inMax`
 *                       cfg = &SimScaleCfg, state = nullptr.
 *
 *   Typical three-proc chain (THROTTLE_BUS → RPM_BUS):
 *
 *     sim_center_fn  \u2192  signed(throttle \u2212 CbusNeutral)  range [\u2212500..+500] as int16
 *     sim_abs_fn     \u2192  |signed|, optOutDCh = nullopt      range [0..CbusNeutral]
 *     sim_scale_fn   \u2192  magnitude \u00d7 maxRpm / CbusNeutral    range [0..maxRpm]
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // SimProc, SimScaleCfg
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 1. SIMPROC FUNCTIONS
// =============================================================================

/**
 * @brief Signed center SimProc — `value = (uint16_t)(int16_t)(value − cfg->neutral)`.
 *
 * @details Converts a neutral-centered ComBus position to its **signed**
 *   deviation from neutral, packed as two's complement in uint16_t.
 *   Positive values (FWD side) are unchanged; negative values (REV side) wrap
 *   via two's complement (e.g. −300 → 0xFED4).
 *
 *   Chain with `sim_abs_fn` to extract the magnitude and optionally
 *   capture the direction as a DigitalComBusID side effect.
 *   cfg = &SimCenterCfg (state must be nullptr).
 *   Does NOT set `claimed`.
 *
 * @param proc    SimProc descriptor — `cfg` cast to `const SimCenterCfg*`.
 * @param value   In: unsigned ComBus [0..CbusMaxVal].  Out: signed packed int16.
 * @param bus     Not read.
 * @param claimed Not modified.
 */
void sim_center_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed, ChanOwner chanOwner);

/**
 * @brief Absolute-value SimProc \u2014 `value = |reinterpret<int16_t>(value)|`.
 *
 * @details Interprets `value` as a two's complement int16_t (as produced by
 *   `sim_center_fn`), computes its absolute value, and writes it back.
 *   **cfg-free** (`proc->cfg` must be nullptr).
 *   Optionally writes the sign as a digital side effect via `proc->optOutCh` (when `isDigital == true`):
 *   HIGH if input was positive (FWD), LOW if negative (REV).
 *   std::nullopt = no side effect.
 *
 *   Output range: [0..CbusNeutral] after a `sim_center_fn` upstream.
 *   state must be nullptr.
 *   Does NOT set `claimed`.
 *
 * @param proc    SimProc descriptor — reads `proc->optOutCh` (digital) for sign side effect; `cfg` ignored.
 * @param value   In: signed-packed int16 (from sim_center_fn).  Out: magnitude.
 * @param bus     Written when `proc->optOutCh.has_value()` and `isDigital == true`.
 * @param claimed Not modified.
 */
void sim_abs_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed, ChanOwner chanOwner);

/**
 * @brief Linear scale SimProc \u2014 `value = value \u00d7 outMax / inMax`.
 *
 * @details Rescales `value` from [0..inMax] to [0..outMax].  No clamping \u2014
 *   caller must ensure input does not exceed inMax.
 *   state must be nullptr.
 *   Does NOT set `claimed`.
 *
 * @param proc    SimProc descriptor \u2014 `cfg` cast to `const SimScaleCfg*`.
 * @param value   In/Out: value to rescale.
 * @param bus     Not read.
 * @param claimed Not modified.
 */
void sim_scale_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed, ChanOwner chanOwner);

/**
 * @brief Drive-state observer SimProc — encodes direction and writes DRIVE_STATE_BUS.
 *
 * @details Reads the post-ramp bipolaire `value` and compares it to
 *   `SimDriveStateCfg::neutral` to determine direction:
 *   - value > neutral  → DriveState::kDriveFwd
 *   - value < neutral  → DriveState::kDriveRev
 *   - value == neutral → DriveState::kStanding
 *
 *   Encodes the result via DriveStateBus::encode() and writes it to
 *   `proc->optOutCh` (analog) (e.g. DRIVE_STATE_BUS).
 *   Does **NOT** modify `value` — pure side-effect observer.
 *   Does NOT set `claimed`.
 *   cfg = &SimDriveStateCfg, state = nullptr.
 *
 * @param proc    SimProc descriptor — `cfg` cast to `const SimDriveStateCfg*`;
 *                `optOutCh` (analog) = destination analog channel.
 * @param value   Read-only — post-ramp bipolaire position [0..CbusMaxVal].
 * @param bus     Written via `optOutCh` when has_value() and isDigital == false.
 * @param claimed Not modified.
 */
void sim_drive_state_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed, ChanOwner chanOwner);


// EOF sim_math.h
