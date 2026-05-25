/******************************************************************************
 * @file  sim_subgear_btn.h
 * @brief SimProc function — sub-gear mode button handler.
 *
 * @details `sim_subgear_btn_fn()` tracks three digital buttons (SET, UP, DOWN)
 *   via rising-edge detection and advances a sub-gear index:
 *   - SET: toggles sub-gear mode (0 ↔ 1).
 *   - UP:  increments sub-gear index (when mode active, up to subGearCount).
 *   - DOWN: decrements sub-gear index (when mode active, down to 1).
 *
 *   Input  channels : secInCh[0]=SUBGEAR_SET_BTN, [1]=SUBGEAR_UP_BTN, [2]=SUBGEAR_DOWN_BTN.
 *   Output channel  : optSecOutCh = SUBGEAR_BUS (runner commits after fn).
 *
 *   The primary `value` is not read or modified — this proc is a side-effect
 *   emitter only.  Does NOT set `claimed`.
 *
 *   Placement: BEFORE `sim_gear_fn` in the gear channel so the FSM reads the
 *   freshly-written SUBGEAR_BUS value via its secInCh[1].
 *
 *   Typical declaration (in vehicle sim_config.cpp):
 *   @code
 *     { .name="subgear_btn",
 *       .secInCh     = { DigitalComBusID::SUBGEAR_SET_BTN,
 *                        DigitalComBusID::SUBGEAR_UP_BTN,
 *                        DigitalComBusID::SUBGEAR_DOWN_BTN },
 *       .optSecOutCh = AnalogComBusID::SUBGEAR_BUS,
 *       .fn          = sim_subgear_btn_fn,
 *       .cfg         = &kSubGearBtnCfg,
 *       .state       = &gSubGearBtnState },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>  // SimProc, SimSubGearBtnCfg, SimSubGearBtnState


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Sub-gear button handler — assigned to `SimProc::fn`.
 *
 * @details Matches `CbProcFn` signature.  Reads three secondary inputs
 *   (SET/UP/DOWN) via `proc->secInValue[]`; writes `proc->secOutValue` with
 *   the new sub-gear index; runner commits to `proc->optSecOutCh` (SUBGEAR_BUS).
 *
 *   Does NOT modify `value` or set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `const SimSubGearBtnCfg*`,
 *                `state` cast to `SimSubGearBtnState*`.
 * @param value   Not modified.
 * @param claimed Not modified.
 */
void sim_subgear_btn_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner chanOwner);

// EOF sim_subgear_btn.h
