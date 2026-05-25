/******************************************************************************
 * @file  cb_ramp_struct.h
 * @brief Config and state structs for cb_ramp_fn — single-axis inertia ramp.
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. CONFIG STRUCT
// =============================================================================

/**
 * @brief Static configuration for a single-axis inertia ramp CbProc.
 *
 * @details Assigned to `CbProc::cfg` (as `const void*`); cast back to
 *   `const CbRampCfg*` inside `cb_ramp_fn()`.
 *
 *   Multi-instance: one CbProc entry per axis that needs inertia
 *   (e.g. DUMP_BUS → DUMP_RAMPED_BUS, STEERING_BUS → STEERING_RAMPED_BUS).
 *   The downstream `DcDevice` reads the ramped output channel.
 */
struct CbRampCfg {
    uint16_t rampTimeMs;                    ///< Default period between ramp steps (ms).
    uint16_t accelSteps;                    ///< ComBus units per step when moving away from neutral (both directions if accelDownSteps == 0).
    uint16_t accelDownSteps = 0u;           ///< ComBus units per step when moving in the NEGATIVE direction away from neutral.
                                            ///<   0 = symmetric (falls back to accelSteps).
    uint16_t brakeSteps;                    ///< ComBus units per step when moving toward neutral.
    uint16_t neutralBand;                   ///< ComBus units around CbusNeutral treated as zero (0 = no dead-band).
    bool     resetRamp = false;             ///< Set by a preceding proc (e.g. cb_gear_ramp_fn) on gear change.
                                            ///<   cb_ramp_fn resets the ramp timer and clears this flag.
                                            ///<   currentPos is preserved — continuity of position across the reset.
};


// =============================================================================
// 2. STATE STRUCT
// =============================================================================

/**
 * @brief Mutable runtime state for a ramp CbProc.
 *
 * @details Written exclusively by `cb_ramp_fn()`.
 *   Zero-initialised by default construction — `lastUpdateMs == 0` triggers
 *   a self-init to CbusNeutral on the first call.
 *   Do NOT use `currentPos == 0` as sentinel — 0 is a valid position (full negative).
 */
struct CbRampState {
    uint16_t          currentPos;       ///< Current inertial position in ComBus domain [0..CbusMaxVal].
    uint32_t          lastUpdateMs;     ///< millis() timestamp of last ramp step.
                                        ///<   Also used as first-call sentinel: 0 = never initialised.
};

// EOF cb_ramp_struct.h
