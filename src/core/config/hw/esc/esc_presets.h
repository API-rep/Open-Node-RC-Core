/******************************************************************************
 * @file esc_presets.h
 * @brief Ready-made EscInertiaConfig presets for common vehicle drive types.
 *
 * @details Each preset is a `constexpr EscInertiaConfig` value.  Machine
 *   config files include this header and assign a preset address to
 *   `DcDevice::motion` for any device that needs inertia simulation.
 *
 *   Preset naming convention: `kMotion_<DriveType>_<Variant>`.
 *
 *   Range field values are derived from `esc_calibrate()` defaults:
 *   neutral = 32767, full-range halfSpan = 32767, no dead-band, no
 *   reverse extension.  Override via a custom `EscInertiaConfig` in the
 *   machine config when the hardware ESC calibration differs.
 *****************************************************************************/
#pragma once

#include <struct/esc_inertia_struct.h>


// =============================================================================
// 1. TRACTION PRESETS
// =============================================================================

/**
 * @brief Traction preset for a multi-axle wheeled truck (slow, heavy).
 *
 * @details 3-gear ramp profile (20 / 55 / 80 ms per step) with moderate
 *   braking response.  Suitable for 1:16 construction-vehicle traction
 *   motors at the default 200 Hz ComBus rate.
 */
constexpr EscInertiaConfig kMotion_Traction_Truck {
    .mode             = MotionMode::TRACTION_FSM,

    // --- ComBus input range (neutral = 32767, full ±32767, no dead-band) ---
    .cbusNeutral      = 32767u,
    .cbusMax          = 65535u,
    .cbusMin          = 0u,
    .cbusMaxNeutral   = 32767u,
    .cbusMinNeutral   = 32767u,
    .cbusMaxLimit     = 65535u,
    .cbusMinLimit     = 0u,

    // --- ESC hardware range (same as input for PWM_HBRIDGE direct mapping) ---
    .escMax           = 65535u,
    .escMin           = 0u,
    .escMaxNeutral    = 32767u,
    .escMinNeutral    = 32767u,

    // --- Ramp timing ---
    .rampTimeFirstMs  = 20u,
    .rampTimeSecondMs = 55u,
    .rampTimeThirdMs  = 80u,
    .crawlerRampTimeMs = 2u,

    // --- Ramp increments (ComBus units per step) ---
    .brakeSteps       = 655u,   ///< ~1 % of full range per step
    .accelSteps       = 327u,   ///< ~0.5 % of full range per step
    .brakeMargin      = 1310u,  ///< ~2 % of full range

    // --- Scaling ---
    .globalAccelPct   = 100u,
    .lowRangePct      = 50u,
    .autoRevAccelPct  = 100u,

    // --- No output-curve correction for direct H-bridge mapping ---
    .linearizeFn      = nullptr,

    // --- ComBus output filled at init (not in preset) ---
    .comBus           = nullptr,
    .owner            = ChanOwner::NONE,
};


// =============================================================================
// 2. HYDRAULIC PRESETS
// =============================================================================

/**
 * @brief Hydraulic actuator preset — slow linear cylinder.
 *
 * @details Simple slew-rate limiting (RAMP_SIMPLE).  No gear/engine coupling.
 *   200 ms ramp mimics the hydraulic oil flow delay of a slow dump cylinder.
 */
constexpr EscInertiaConfig kMotion_Hydraulic_Slow {
    .mode             = MotionMode::RAMP_SIMPLE,

    .cbusNeutral      = 32767u,
    .cbusMax          = 65535u,
    .cbusMin          = 0u,
    .cbusMaxNeutral   = 32767u,
    .cbusMinNeutral   = 32767u,
    .cbusMaxLimit     = 65535u,
    .cbusMinLimit     = 0u,

    .escMax           = 65535u,
    .escMin           = 0u,
    .escMaxNeutral    = 32767u,
    .escMinNeutral    = 32767u,

    .rampTimeFirstMs  = 200u,   ///< Single ramp time (RAMP_SIMPLE uses rampTimeFirstMs)
    .rampTimeSecondMs = 200u,
    .rampTimeThirdMs  = 200u,
    .crawlerRampTimeMs = 200u,

    .brakeSteps       = 327u,
    .accelSteps       = 327u,
    .brakeMargin      = 0u,

    .globalAccelPct   = 100u,
    .lowRangePct      = 100u,
    .autoRevAccelPct  = 100u,

    .linearizeFn      = nullptr,
    .comBus           = nullptr,
    .owner            = ChanOwner::NONE,
};


// =============================================================================
// 3. STEERING PRESETS
// =============================================================================

/**
 * @brief Steering actuator preset — fast slew-rate for responsive steering.
 *
 * @details RAMP_SIMPLE with a 50 ms ramp — fast enough for a motor-driven
 *   steering rack while smoothing out stick jitter.
 */
constexpr EscInertiaConfig kMotion_Steer {
    .mode             = MotionMode::RAMP_SIMPLE,

    .cbusNeutral      = 32767u,
    .cbusMax          = 65535u,
    .cbusMin          = 0u,
    .cbusMaxNeutral   = 32767u,
    .cbusMinNeutral   = 32767u,
    .cbusMaxLimit     = 65535u,
    .cbusMinLimit     = 0u,

    .escMax           = 65535u,
    .escMin           = 0u,
    .escMaxNeutral    = 32767u,
    .escMinNeutral    = 32767u,

    .rampTimeFirstMs  = 50u,
    .rampTimeSecondMs = 50u,
    .rampTimeThirdMs  = 50u,
    .crawlerRampTimeMs = 50u,

    .brakeSteps       = 1310u,  ///< ~2 % — fast centering
    .accelSteps       = 1310u,
    .brakeMargin      = 0u,

    .globalAccelPct   = 100u,
    .lowRangePct      = 100u,
    .autoRevAccelPct  = 100u,

    .linearizeFn      = nullptr,
    .comBus           = nullptr,
    .owner            = ChanOwner::NONE,
};

// EOF esc_presets.h
