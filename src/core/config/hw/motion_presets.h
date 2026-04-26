/******************************************************************************
 * @file motion_presets.h
 * @brief Ready-made MotionConfig presets — generic vehicle-class library.
 *
 * @details Each entry is a `constexpr MotionConfig` value.  Machine config
 *   files include this header and assign a preset address to the device's
 *   motion config pointer for any device that needs inertia simulation or
 *   slew-rate limiting.
 *
 *   Preset naming convention: `k<Class>_<Category>`.
 *   Sub-config naming: `k<Class>_<Category><SubConfigType>`.
 *****************************************************************************/
#pragma once

#include <struct/motion_struct.h>
#include <core/system/combus/combus_res.h>  // CbusMaxVal, CbusNeutral, pctToCbus


// =============================================================================
// 0. SHARED SUB-CONFIGS  (reused across multiple presets)
// =============================================================================

  /// Full ComBus range — no hardware soft-limit restriction.
static constexpr MotionHwMargin kHw_Full {
    .maxHwVal = CbusMaxVal,
    .minHwVal = CbusMinVal,
};

  /// Zero dead-band — both edges at neutral; driver-side handles clamping.
static constexpr MotionDeadBand kBand_None {
    .maxNeutral = CbusNeutral,
    .minNeutral = CbusNeutral,
};

  /// ±3 % dead-band — absorbs stick neutral drift; suitable for electric actuators.
static constexpr MotionDeadBand kBand_3pct {
    .maxNeutral = static_cast<combus_t>(CbusNeutral + pctToCbus(3)),
    .minNeutral = static_cast<combus_t>(CbusNeutral - pctToCbus(3)),
};


// =============================================================================
// 1. TRACTION — HEAVY WHEELED  (ADT, 6WD construction hauler class)
// =============================================================================


/**
 * @brief Gear timing — generic heavy construction-vehicle defaults.
 *
 * @details Conservative 1st-gear ramp suits a loaded multi-axle hauler.
 *   Decrease `rampTimeFirstMs` (e.g. 10 ms) for a snappier response;
 *   increase it (e.g. 60 ms) on slippery ground with a full load.
 *   Gear advance (1→2→3) is always active — set `MotionConfig::gear = nullptr`
 *   to revert to simple-ramp mode with no gear model.
 */
static constexpr MotionGear kTraction_HeavyGear {
    .rampTimeFirstMs  = 40u,   ///< Heavy hauler — ~2 s from neutral to full forward.
    .rampTimeSecondMs = 80u,   ///< Mid-speed ramp (2× 1st-gear period).
    .rampTimeThirdMs  = 120u,  ///< High-speed ramp (3× 1st-gear period).
    .globalAccelPct   = 100u,  ///< Nominal rate — adjust to taste.
};

/**
 * @brief Inertia model — firm braking, gradual acceleration.
 *
 * @details Scaled from diyGuy generic heavy-vehicle defaults
 *   (accelSteps=3, brakeSteps=30 over a 500-unit ESC half-range
 *   → ~0.6 % and ~6 % of ComBus half-range respectively).
 *   Braking is 6× stronger than acceleration — matches a loaded hauler
 *   that coasts slowly but stops firmly.
 */
static constexpr MotionInertia kTraction_HeavyInertia {
    .accelSteps  = pctToCbus(1),  ///< ~0.6 % of half-range per step — gradual (≈ diyGuy accelSteps=3).
    .brakeSteps  = pctToCbus(6),  ///< ~6 % of half-range per step — firm   (≈ diyGuy brakeSteps=30).
    .brakeMargin = pctToCbus(2),  ///< Hold ≥ 2 % from neutral during engine braking (≈ diyGuy brakeMargin=10).
};

/**
 * @brief Asymmetric travel margin for heavy traction — full forward, capped reverse.
 *
 * @details Reverse is limited to 50 % of the full half-range, preventing
 *   runaway on slopes and matching construction-site safety practice.
 *   Forward is unrestricted (hardware limit applies).
 */
static constexpr MotionMargin kTraction_HeavyMargin {
    .maxVal = CbusMaxVal,                       ///< Full forward — no restriction.
    .minVal = static_cast<combus_t>(CbusNeutral - pctToCbus(50)),  ///< 50 % reverse cap.
};

/**
 * @brief Traction preset — heavy wheeled construction vehicle.
 *
 * @details Algorithm: traction FSM (`gear` + `inertia` set, `ramp` nullptr).
 *   Suitable for ADTs, 6WD haulers, heavy loaders.  Assign to traction
 *   `DcDevice` entries.
 */
static constexpr MotionConfig kTraction_Heavy {
    .hw      = &kHw_Full,
    .margin  = &kTraction_HeavyMargin,           ///< Asymmetric: full forward, 50 % reverse.
    .band    = &kBand_None,
    .ramp    = nullptr,                          ///< Traction mode: gear + inertia algorithm.
    .gear    = &kTraction_HeavyGear,
    .inertia = &kTraction_HeavyInertia,
};


// =============================================================================
// 2. STEERING — ELECTRIC ACTUATOR  (slow DC linear actuator, articulation / tilt)
// =============================================================================

/**
 * @brief Simple ramp profile for a slow DC linear steering actuator.
 *
 * @details Symmetric — identical slew rate in both directions.  Fast
 *   enough to give a responsive steering feel; gentle enough to avoid
 *   overloading the actuator at full lock.
 *   Tune `rampTimeMs` if steering is too jerky (increase) or sluggish (decrease).
 */
static constexpr MotionRamp kSteer_Electric_heavyRamp {
    .rampTimeMs  = 30u,           ///< 30 ms between ramp steps.
    .accelSteps  = pctToCbus(2),  ///< ~2 % of half-range per step.
    .brakeSteps  = pctToCbus(2),  ///< Symmetric centering rate.
};

/**
 * @brief Steering preset — slow electric linear actuator.
 *
 * @details Algorithm: simple ramp (`ramp` set, `gear` / `inertia` nullptr).
 *   Suitable for DC linear actuators used for articulation steering or tilt.
 *   Assign to the steering `DcDevice`.
 */
static constexpr MotionConfig kSteer_Electric_heavy {
    .hw      = &kHw_Full,
    .margin  = nullptr,             ///< Full hardware range — no soft limit.
    .band    = &kBand_3pct,
    .ramp    = &kSteer_Electric_heavyRamp,
    .gear    = nullptr,             ///< Simple ramp mode — no traction FSM.
    .inertia = nullptr,             ///< Simple ramp mode — no traction FSM.
};

// EOF motion_presets.h
