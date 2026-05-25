/*!****************************************************************************
 * @file  ctrl_toggle.h
 * @brief Toggle CtrlProcFn — rising-edge toggle with optional speed-gate on engage.
 *
 * @details The toggle proc:
 *   - Rising edge of `value`: if `active == false`, engage (speed guard permitting).
 *   - Rising edge of `value`: if `active == true`, disengage immediately.
 *   - `isDrived` guard is enforced upstream by the runner.
 *   - Speed guard: skip engagement when `analogBus[speedCh].value > maxEngageSpd`
 *     (only evaluated when `speedCh` is set and `maxEngageSpd > 0`).
 *
 *   Output: writes `active` into `value` — the runner then propagates it to `outCh`.
 *
 *   @code
 *     static constexpr CtrlToggleCfg kCfg { .speedCh = AnalogComBusID::RPM_BUS,
 *                                            .maxEngageSpd = 200u };
 *     static CtrlToggleState gState {};
 *     CtrlProc myProc { "toggle", ctrl_toggle_fn, &kCfg, &gState };
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/ctrl_struct.h>  // CtrlProc, CtrlToggleCfg, CtrlToggleState, CtrlProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Toggle CtrlProcFn — rising-edge toggle with optional speed-gate on engage.
 *
 * @param proc    Proc descriptor: `cfg → const CtrlToggleCfg*`, `state → CtrlToggleState*`.
 * @param value   Button state (in) — after the call, carries the resulting `active` flag.
 * @param bus     Full ComBus — speed channel read for the engage guard.
 * @param claimed Unused — the runner writes `outCh` after this proc returns.
 * @param owner   Unused — no direct bus writes inside this proc.
 */
void ctrl_toggle_fn(CtrlProc* proc, bool& value, ComBus& bus,
                    bool& claimed, ChanOwner owner);

// EOF ctrl_toggle.h
