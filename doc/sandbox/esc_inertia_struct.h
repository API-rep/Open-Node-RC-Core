// [SANDBOX — migration reference only, not compiled]
// Original: include/struct/esc_inertia_struct.h
// Delete migrated sections as they are covered by the new motion architecture.
// See doc/sandbox/README.md for the migration checklist.

/******************************************************************************
 * @file esc_inertia_struct.h
 * @brief Persistent data types for the vehicle-inertia motion controller.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_struct.h>    // ChanOwner, ComBus


// =============================================================================
// 1. MOTION MODE  →  migrates to MotionLayer enum in motion_struct.h
// =============================================================================

enum class MotionMode : uint8_t {
    RAMP_SIMPLE   = 0,  ///< Slew-rate only — hydraulics, steering actuators
    TRACTION_FSM  = 1,  ///< 5-state drive FSM — wheel or track traction
};


// =============================================================================
// 2. LINEARIZATION HOOK  →  migrates into MotionFsm sub-config
// =============================================================================

using EscLinearizeFn = uint16_t (*)(uint16_t val);


// =============================================================================
// 3. CONFIGURATION  →  splits into MotionHw / MotionMargin / MotionBand /
//                       MotionRamp / MotionGear / MotionFsm + MotionConfig
// =============================================================================

struct EscInertiaConfig {
    MotionMode mode = MotionMode::RAMP_SIMPLE;

    // --- RC input range ---
    uint16_t cbusNeutral;
    uint16_t cbusMax;
    uint16_t cbusMin;
    uint16_t cbusMaxNeutral;     // → MotionBand
    uint16_t cbusMinNeutral;     // → MotionBand
    uint16_t cbusMaxLimit;       // → MotionMargin
    uint16_t cbusMinLimit;       // → MotionMargin

    // --- ESC hardware range ---
    uint16_t escMax;             // → MotionHw.maxHwVal
    uint16_t escMin;             // → MotionHw.minHwVal
    uint16_t escMaxNeutral;      // → MotionBand
    uint16_t escMinNeutral;      // → MotionBand

    // --- Ramp timing ---
    uint16_t rampTimeFirstMs;    // → MotionRamp / MotionGear
    uint16_t rampTimeSecondMs;   // → MotionGear
    uint16_t rampTimeThirdMs;    // → MotionGear
    uint16_t crawlerRampTimeMs;  // → MotionGear

    // --- Ramp increments ---
    uint16_t brakeSteps;         // → MotionRamp / MotionFsm
    uint16_t accelSteps;         // → MotionRamp / MotionFsm
    uint16_t brakeMargin;        // → MotionFsm

    // --- Scaling percentages ---
    uint8_t  globalAccelPct;     // → MotionGear
    uint8_t  lowRangePct;        // → MotionGear
    uint8_t  autoRevAccelPct;    // → MotionGear

    // --- Hook ---
    EscLinearizeFn linearizeFn;  // → MotionFsm

    // --- ComBus output ---
    ComBus*   comBus;            // → MotionConfig.comBus
    ChanOwner owner;             // → MotionConfig.owner
};


// =============================================================================
// 4. RUNTIME STATE  →  migrates to MotionRuntime in motion_struct.h
// =============================================================================

struct EscInertiaRuntime {
    uint16_t  cbusPos       = 32767u;
    int8_t    driveState    = 0;
    uint16_t  driveRampRate = 1u;
    uint16_t  brakeRampRate = 1u;
    uint8_t   driveRampGain = 1u;
    uint32_t  rampMillis    = 0u;
};


// =============================================================================
// 5. OUTPUT SNAPSHOT  →  migrates to MotionState in motion_struct.h
// =============================================================================

struct EscInertiaState {
    uint16_t cbusPos;
    uint16_t escCbusVal;
    uint16_t currentSpeed;
    bool     escIsBraking;
    bool     escInReverse;
    bool     escIsDriving;
    bool     brakeDetect;
    bool     airBrakeTrigger;
};

// EOF sandbox/esc_inertia_struct.h
