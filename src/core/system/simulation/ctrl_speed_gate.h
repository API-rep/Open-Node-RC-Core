/*!****************************************************************************
 * @file  ctrl_speed_gate.h
 * @brief Speed-gate CbProcFn — suppresses engagement above a speed threshold.
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

#include <struct/ctrl_struct.h>  // CbProc, CtrlSpeedGateCfg, CbProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Speed-gate CbProcFn — suppresses engagement when speed > threshold.
 *
 * @param proc    Proc descriptor: `cfg → const CtrlSpeedGateCfg*`, `state → nullptr`.
 *                `secInCh[0]` = RPM_BUS (speed); `secInCh[1]` = DIRECT_DRIVE (active).
 * @param value   Button state (in/out) as uint16_t (0 = off) — set to 0 when blocked.
 * @param claimed Unused.
 * @param owner   Unused.
 */
void ctrl_speed_gate_fn(CbProc* proc, uint16_t& value,
                        bool& claimed, ChanOwner owner);

// EOF ctrl_speed_gate.h
