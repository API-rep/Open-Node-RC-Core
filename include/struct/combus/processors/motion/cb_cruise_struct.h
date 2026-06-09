/******************************************************************************
 * @file  cb_cruise_struct.h
 * @brief Throttle hold (cruise-control) CbProc — config and state structures.
 *
 * @details Used by `cb_cruise_fn` in `src/core/system/combus/processors/motion/`.
 *
 *   Two modes:
 *     Normal mode (holdSpeed = false):
 *       `state->active = true` : HOLD — clamps value at `heldValue` (floor hold).
 *         Speed above held: track upward; speed below: hold floor.
 *         Braking (extBrakeSteps > 0 in dynRampCfg): deactivate, sync ramp.
 *       `state->active = false` : passthrough — value unchanged.
 *
 *     holdSpeed mode (holdSpeed = true):
 *       Auto-activated via `inCh = SUBGEAR_BUS` (nonzero = active).
 *       Speed above held: track upward (new held); speed below: hold floor.
 *       Braking: track downward (new held = braked-to speed), pass through.
 *       Deactivation: sync `rampState->currentPos = heldValue` — no jerk.
 *
 *   `state->updateReq = true` : update held = current value on next cycle.
 *     Set externally (button) or by internal logic (future).
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus/processors/motion/cb_ramp_struct.h>  // CbRampCfg, CbRampState


// =============================================================================
// 1. PROC CONFIG
// =============================================================================

/**
 * @brief Configuration for `cb_cruise_fn` — throttle hold.
 *
 * @details This struct is ROM-resident (`static const`) — all mutable activation
 *   state lives in `CbCruiseState`. `dynRampCfg` and `rampState` are const
 *   pointers to mutable objects.
 */
struct CbCruiseCfg {
    bool              holdSpeed;  ///< false = normal (floor + deactivate on brake);
                                  ///<   true  = holdSpeed (adaptive watermark, auto via inCh).
    const CbRampCfg*  dynRampCfg; ///< Linked ramp — read extBrakeSteps for braking detection;
                                  ///<   nullptr = no brake detect.
    CbRampState*      rampState;  ///< Linked ramp state — sync currentPos on deactivation;
                                  ///<   nullptr = skip sync.
};


// =============================================================================
// 2. PROC STATE
// =============================================================================

/**
 * @brief Runtime state for `cb_cruise_fn`.
 */
struct CbCruiseState {
    bool     active;    ///< Activation flag (normal mode only).
                        ///<   Set/cleared externally. In holdSpeed mode, driven by inCh.
    uint16_t heldValue; ///< Held throttle value in bipolar domain [0..CbusMaxVal].
                        ///<   Set on rising `active` edge, updated in holdSpeed mode.
    bool     wasActive; ///< Previous active state — rising-edge detection.
    bool     updateReq; ///< Pending "update cruise speed": heldValue ← current on next cycle.
                        ///<   Write externally (button map) or by internal logic.
};

// EOF cb_cruise_struct.h
