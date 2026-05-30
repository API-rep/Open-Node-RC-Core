/******************************************************************************
 * @file  cb_brake.h
 * @brief CbProc functions — braking pipeline (brake-input normalize / reverse-brake /
 *        dynamic ramp deceleration).
 *
 * @details Three functions for the three dedicated procs.  See
 *   cb_brake_struct.h for the full design rationale.
 *
 *   Typical chain placement (inside kThrottleProcs):
 *   @code
 *     // After cb_ramp_fn + cb_dir_fn:
 *     { .name="b-in",   .inCh=BRAKE_BUS,    .fn=cb_brake_fn,  .state=&gBrakeState          },
 *     { .name="brake",  .inCh=THROTTLE_BUS, .fn=cb_rev_brake_fn, .cfg=&kBrakeCfg,
 *                       .outCh=BRAKE_BUS,                         .state=&gBrakeState          },
 *     // ... cb_center_fn, cb_abs_fn, cb_scale_fn ... (no brake-sub needed)
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                               // CbProc, CbProcFn, ChanOwner
#include <struct/combus/processors/motion/cb_brake_struct.h>        // CbBrakeCfg, CbBrakeState


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Brake-input normalizer — cache BRAKE_BUS into CbBrakeState::brakeVal.
 *
 * @details Reads `proc->inValue` (BRAKE_BUS, 0..analogBusMaxVal).
 *   Normalizes to 0..CbusNeutral via `>> 1`.
 *   Observer: does NOT modify `value` or `claimed`.
 *
 * @param proc   CbProc descriptor — `state` cast to `CbBrakeState*`.
 *               `inCh` = BRAKE_BUS.  `cfg` not used.
 * @param value   Not modified.
 * @param claimed Not modified.
 * @param owner   Not used.
 */
void cb_brake_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner owner);

/**
 * @brief Reverse-brake merger — compute and commit BRAKE_BUS.
 *
 * @details Reads `value` (post-ramp bipolar, vehicle speed proxy) and
 *   `proc->inValue` (THROTTLE_BUS, raw stick).  Computes reverse-brake
 *   force proportional to opposing stick deflection, merges with
 *   CbBrakeState::brakeVal (set by cb_brake_fn this same cycle, scaled
 *   by CbBrakeCfg::brakeScalePct), applies post-brake hold timer, clamps to
 *   CbBrakeCfg::maxBrake.  Also dynamically interpolates dynRampCfg->brakeSteps
 *   so the ramp decelerates faster while brake is held.
 *   Writes result to `proc->outValue`; runner commits to `outCh` = BRAKE_BUS.
 *   Observer: does NOT modify `value` or `claimed`.
 *
 * @param proc   CbProc descriptor — `cfg` = `CbBrakeCfg*`, `state` = `CbBrakeState*`.
 *               `inCh` = THROTTLE_BUS.  `outCh` = BRAKE_BUS.
 * @param value   Not modified (post-ramp position, used as vehicle speed proxy).
 * @param claimed Not modified.
 * @param owner   Not used.
 */
void cb_rev_brake_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner owner);

// EOF cb_brake.h
