/******************************************************************************
 * @file  cb_brake_struct.h
 * @brief Config and state structs for the brake processor pair
 *        (cb_brake_fn / cb_rev_brake_fn).
 *
 * @details Two-proc design sharing CbBrakeState:
 *
 *   Proc "b-in"  (cb_brake_fn)
 *     inCh = BRAKE_BUS  — caches normalized brake input (0..65535 → 0..CbusNeutral)
 *     into state->brakeVal each cycle.  Observer; does not modify value.
 *
 *   Proc "brake" (cb_rev_brake_fn)
 *     inCh = THROTTLE_BUS — computes reverse-brake magnitude from post-ramp
 *     value vs. raw stick, merges with scaled state->brakeVal, applies post-brake
 *     hold timer, writes final result to outCh = BRAKE_BUS.  Also writes
 *     CbBrakeCfg::dynRampCfg->extBrakeSteps proportional to finalBrake so the
 *     ramp itself decelerates faster while brake is held (brakeSteps in CbRampCfg
 *     remains unchanged as the permanent coasting baseline).
 *
 *   Reverse-brake logic (inside cb_rev_brake_fn):
 *     rampVal  = value (post-ramp, bipolar — represents simulated speed).
 *     stickVal = proc->inValue (THROTTLE_BUS — raw stick, same domain).
 *     if rampVal > CbusNeutral && stickVal < CbusNeutral → brake (going FWD, stick opposing)
 *     if rampVal < CbusNeutral && stickVal > CbusNeutral → brake (going REV, stick opposing)
 *     brakeForce = |stickVal − CbusNeutral|  — proportional to stick deflection.
 *
 *   Post-brake hold:
 *     When reverse-braking disengages (ramp crosses back to stick direction),
 *     BRAKE_BUS is held at maxBrake for reverseHoldMs ms.  This prevents an
 *     immediate direction change after the vehicle has been braked to a stop.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus/processors/motion/cb_ramp_struct.h>  // CbRampCfg (dynRampCfg)


// =============================================================================
// 1. CONFIG STRUCTS
// =============================================================================

/**
 * @brief Static configuration for cb_brake_fn / cb_rev_brake_fn.
 *
 * @details Assigned to `CbProc::cfg` of the "brake" proc (cb_rev_brake_fn);
 *   the "l2" proc shares `CbBrakeState` only (no cfg needed).
 */
struct CbBrakeCfg {
    uint16_t    maxBrake;          ///< Maximum merged brake force [0..CbusNeutral].
                                   ///<   CbusNeutral = full stop at maximum opposing deflection.
    uint16_t    reverseHoldMs;     ///< Duration (ms) maxBrake is held after reverse-brake disengages. 0 = no hold.
    CbRampCfg*  dynRampCfg;        ///< Mutable ramp dynCfg — extBrakeSteps updated each cycle.  nullptr = disabled.
    uint16_t    maxBrakeStep;      ///< extBrakeSteps ceiling: value written at full brake force (0..CbusNeutral).
    uint8_t     brakeScalePct;        ///< Brake-input contribution scale (0..100 %).
                                   ///<   100 = full deflection reaches maxBrake.
                                   ///<    50 = full deflection reaches maxBrake / 2.
    uint8_t     revBrakeScalePct;  ///< Reverse-brake magnitude multiplier (100 = 1×, 150 = 1.5×).
                                   ///<   Applied before final clamp to maxBrake.
    uint16_t    revBrakeDeadBand = 0u; ///< Minimum revBrake force required to arm revBraking state.
                                   ///<   Prevents false hold triggers from stick noise or brief
                                   ///<   neutral crossings when the user simply returns to neutral.
                                   ///<   Typical: pctToCbus(5).  0 = any deflection arms (original behaviour).
};

// =============================================================================
// 2. STATE STRUCT
// =============================================================================

/**
 * @brief Mutable runtime state shared by cb_brake_fn and cb_rev_brake_fn.
 *
 * @details Zero-initialised by default construction.
 *   cb_brake_fn  writes brakeVal each cycle.
 *   cb_rev_brake_fn reads brakeVal (scaled by brakeScalePct) and writes holdUntilMs / revBraking.
 */
struct CbBrakeState {
    uint16_t brakeVal    = 0u;     ///< Normalized brake-input cached by cb_brake_fn (0..CbusNeutral).
    uint32_t holdUntilMs = 0u;     ///< millis() timestamp until post-brake hold is active.
    bool     revBraking  = false;  ///< True while reverse-brake is actively engaged.
};

// EOF cb_brake_struct.h
