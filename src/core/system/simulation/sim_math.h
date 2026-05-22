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
 *                       Optional digital side effect via `proc->optOutDCh`:
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
 * @brief Signed center SimProc \u2014 `value = (uint16_t)(int16_t)(value \u2212 CbusNeutral)`.
 *
 * @details Converts a CbusNeutral-centered ComBus position to its **signed**
 *   deviation from neutral, packed as two's complement in uint16_t.
 *   Positive values (FWD side) are unchanged; negative values (REV side) wrap
 *   via two's complement (e.g. \u2212300 \u2192 0xFED4).
 *
 *   Chain with `sim_abs_fn` to extract the magnitude and optionally
 *   capture the direction as a DigitalComBusID side effect.
 *   cfg and state must be nullptr.
 *   Does NOT set `claimed`.
 *
 * @param proc    Not read (cfg must be nullptr).
 * @param value   In: unsigned ComBus [0..CbusMaxVal].  Out: signed packed int16.
 * @param bus     Not read.
 * @param claimed Not modified.
 */
void sim_center_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

/**
 * @brief Absolute-value SimProc \u2014 `value = |reinterpret<int16_t>(value)|`.
 *
 * @details Interprets `value` as a two's complement int16_t (as produced by
 *   `sim_center_fn`), computes its absolute value, and writes it back.
 *   **cfg-free** (`proc->cfg` must be nullptr).
 *   Optionally writes the sign as a digital side effect via `proc->optOutDCh`:
 *   HIGH if input was positive (FWD), LOW if negative (REV).
 *   std::nullopt = no side effect.
 *
 *   Output range: [0..CbusNeutral] after a `sim_center_fn` upstream.
 *   state must be nullptr.
 *   Does NOT set `claimed`.
 *
 * @param proc    SimProc descriptor — reads `proc->optOutDCh`; `cfg` ignored.
 * @param value   In: signed-packed int16 (from sim_center_fn).  Out: magnitude.
 * @param bus     Written when `proc->optOutDCh.has_value()`.
 * @param claimed Not modified.
 */
void sim_abs_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

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
void sim_scale_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);


// EOF sim_math.h
