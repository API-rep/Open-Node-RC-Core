/*!****************************************************************************
 * @file  ctrl_speed_gate.h
 * @brief Speed-gate CtrlProcFn — suppresses engagement above a speed threshold.
 *
 * @details Reads the current speed and the current active state from the bus,
 *   and suppresses the button input (`value = false`) when the vehicle is moving
 *   faster than `maxEngageSpd` and the output is not already active.
 *
 *   Disengagement is always allowed (gate bypasses when `activeCh` is true).
 *
 *   Placement: insert between `ctrl_read_fn` and `ctrl_toggle_fn` in the proc
 *   chain.  `ctrl_toggle_fn` never sees a rising edge when the gate blocks.
 *
 *   @code
 *     static constexpr CtrlSpeedGateCfg kGateCfg {
 *         .speedCh      = AnalogComBusID::RPM_BUS,
 *         .maxEngageSpd = 200u,
 *         .activeCh     = DigitalComBusID::DIRECT_DRIVE,
 *     };
 *     { "speed_gate", ctrl_speed_gate_fn, &kGateCfg, nullptr },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/ctrl_struct.h>  // CtrlProc, CtrlSpeedGateCfg, CtrlProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Speed-gate CtrlProcFn — suppresses engagement when speed > threshold.
 *
 * @param proc    Proc descriptor: `cfg → const CtrlSpeedGateCfg*`, `state → nullptr`.
 * @param value   Button state (in/out) — set to false when gate blocks engagement.
 * @param bus     Full ComBus — speed channel and active state channel are read here.
 * @param claimed Unused.
 * @param owner   Unused.
 */
void ctrl_speed_gate_fn(CtrlProc* proc, bool& value, ComBus& bus,
                        bool& claimed, ChanOwner owner);

// EOF ctrl_speed_gate.h
