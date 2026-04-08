/******************************************************************************
 * @file motion_struct.h
 * @brief Persistent data types for the motion-control pipeline.
 *
 * @details Groups the structs whose instances outlive a single function call
 *   and must be visible at the `include/struct/` layer.
 *
 *   All range and limit fields are in `combus_t` units — never raw angles
 *   or percentages.  Always derive values through the helpers in combus_res.h`:
 *     - `pctToCbus(pct)`          — speed/force percentage → `combus_t` offset
 *     - `angleToCbus(deg, hw)`    — servo angle → absolute `combus_t` value
 *
 *   Four categories of types:
 *
 *   Config     — set once at init, treated as const:
 *                `MotionHwMargin`, `MotionMargin`, `MotionDeadBand`,
 *                `MotionRamp`, `MotionGear`, `MotionInertia`.
 *   Aggregate  — `MotionConfig`: const* pointers to config sub-structs
 *                and the target ComBus channel.
 *   Runtime    — `MotionRuntime`: mutable per-device inertia state.
 *   Output     — `MotionOutput`: per-cycle result of the pipeline.
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <const.h>                          // combus_t
#include <core/system/combus/combus_res.h>  // CbusNeutral (MotionRuntime default)
#include <struct/combus_struct.h>           // ComBus, ChanOwner


// =============================================================================
// 1. CONFIGURATION STRUCTS  (set once at init, treat as const afterwards)
// =============================================================================

/**
 * @brief Hardware output limits — absolute range the pipeline must never exceed.
 *
 * @details Derive limit values with `pctToCbus()` for speed-percentage motors
 *   or `angleToCbus()` for servo / angle-limited actuators:
 *   @code
 *     constexpr MotionHwMargin kDumpHwMargin {
 *         .maxHwVal = CbusNeutral + pctToCbus(100),
 *         .minHwVal = CbusNeutral - pctToCbus(100),
 *     };
 *     constexpr MotionHwMargin kSteerHwMargin {
 *         .maxHwVal = angleToCbus(+60.0f, srvNc120),
 *         .minHwVal = angleToCbus(-60.0f, srvNc120),
 *     };
 *   @endcode
 *   Mandatory fields — no defaults.
 */
struct MotionHwMargin {
    combus_t maxHwVal;  ///< Forward full-travel limit.   e.g. CbusNeutral + pctToCbus(100)
    combus_t minHwVal;  ///< Reverse full-travel limit.   e.g. CbusNeutral - pctToCbus(100)
};


/**
 * @brief Soft operational limits within the hardware range.
 *
 * @details Optional in `MotionConfig` (`margin = nullptr` = use hw limits
 *   directly).  When present, must satisfy `minHwVal ≤ minVal` and
 *   `maxVal ≤ maxHwVal` (checked at init). *
 *   Asymmetric limits are the primary use-case: for example, a traction
 *   device limited to 30 % reverse but full forward:
 *   @code
 *     constexpr MotionMargin kTraction_HeavyMargin {
 *         .maxVal = CbusMaxVal,                      // full forward
 *         .minVal = CbusNeutral - pctToCbus(30),     // 30 % reverse cap
 *     };
 *   @endcode */

struct MotionMargin {
    combus_t maxVal;  ///< Forward soft limit.   e.g. CbusNeutral + pctToCbus(80)
    combus_t minVal;  ///< Reverse soft limit.   e.g. CbusNeutral - pctToCbus(80)
};


/**
 * @brief Neutral dead-band boundaries.
 *
 * @details Commands in `[minNeutral, maxNeutral]` are treated as zero.
 *   A zero-width band (`maxNeutral == minNeutral == CbusNeutral`) is valid
 *   for hardware with no mechanical slack.  Mandatory fields — no defaults.
 */

struct MotionDeadBand {
    combus_t maxNeutral;  ///< Upper edge of dead-band.   e.g. CbusNeutral + pctToCbus(3)
    combus_t minNeutral;  ///< Lower edge of dead-band.   e.g. CbusNeutral - pctToCbus(3)
};


/**
 * @brief Single ramp profiles.
 *
 * @details Combines the step period with per-direction step sizes.
 *   Not referenced by traction — use `MotionGear` + `MotionInertia` instead.
 *   Mandatory fields — no defaults.
 */
struct MotionRamp {
    uint16_t rampTimeMs;  ///< Period between ramp steps (ms).
    uint16_t accelSteps;  ///< `combus_t` increment per step when accelerating.
    uint16_t brakeSteps;  ///< `combus_t` increment per step when braking.
};


/**
 * @brief Gear-set timing profiles.
 *
 * @details Four step-period profiles (1st gear to crawler) plus three global
 *   scaling factors.  Step sizes live in `MotionInertia`.
 *   Mandatory fields — no defaults.
 */
struct MotionGear {
    uint16_t rampTimeFirstMs;    ///< 1st gear ramp period (ms).     e.g. 20
    uint16_t rampTimeSecondMs;   ///< 2nd gear ramp period (ms).     e.g. 50
    uint16_t rampTimeThirdMs;    ///< 3rd gear ramp period (ms).     e.g. 75
    uint16_t rampTimeCrawlerMs;  ///< Crawler / direct mode (ms).    e.g. 2
    uint8_t  globalAccelPct;     ///< Global ramp scale — all gears.  100 = nominal.
    uint8_t  lowRangePct;        ///< Low-range ramp-time multiplier.  50 = half speed.
    uint8_t  autoRevAccelPct;    ///< Auto-reverse acceleration boost.  200 = 2× faster.
};


/**
 * @brief Inertia model parameters.
 *
 * @details Controls how fast the inertial position tracks the command
 *   (accel/brake step sizes) and how close to neutral the output must
 *   stay during braking.  All fields mandatory.
 */
struct MotionInertia {
    uint16_t accelSteps;   ///< `combus_t` increment per step when accelerating.
    uint16_t brakeSteps;   ///< `combus_t` increment per step when braking.
    combus_t brakeMargin;  ///< Min offset from neutral while braking.  e.g. pctToCbus(5)
};


// =============================================================================
// 2. AGGREGATE CONFIG
// =============================================================================

/**
 * @brief Complete motion configuration for one device.
 *
 * @details Selects the active algorithm by which sub-config pointers are set:
 *
 *   A. Simple ramp: hydraulics, steering ...
 *     - `hw`, `band`, `ramp`             : mandatory
 *     - `margin`                         : optional (nullptr = use hw limits)
 *     - `gear`, `inertia`                : must be nullptr
 *
 *   B. Traction — wheel, track drive ...
 *     - `hw`, `band`, `gear`, `inertia`  : mandatory
 *     - `margin`                         : optional (nullptr = use hw limits)
 *     - `ramp`                           : must be nullptr
 *
 *   Instances are `constexpr` and shared across multiple devices (clone
 *   pattern).  Because one preset may be assigned to devices on different
 *   ComBus channels, the target channel is NOT stored here — the caller
 *   (update loop) reads `DcDevice::comChannel` and writes
 *   `runtime.currentPos` to the correct ComBus slot after calling
 *   `motion_update()`.
 *
 *   Declare `constexpr` instances in `motion_presets.h` and assign the
 *   address to `DcDevice::motion`.
 */

struct MotionConfig {
    const MotionHwMargin*  hw;      ///< Hardware limits — mandatory.
    const MotionMargin*    margin;  ///< Soft limits — nullptr = use hw directly.
    const MotionDeadBand*  band;    ///< Dead-band — mandatory.
    const MotionRamp*      ramp;    ///< Slew-rate profile — simple ramp only; nullptr for traction.
    const MotionGear*      gear;    ///< Gear timing — traction only; nullptr for simple ramp.
    const MotionInertia*   inertia; ///< Inertia model — traction only; nullptr for simple ramp.
};



// =============================================================================
// 3. RUNTIME STATE  (mutable, one instance per device)
// =============================================================================

/**
 * @brief Per-instance mutable state for the motion FSM.
 *
 * @details Stored inline in the device struct so each motor instance carries
 *   independent ramp state.  Clone entries must never copy this from their
 *   parent — configs are shared, runtime state is always per-instance.
 */

struct MotionRuntime {
    combus_t currentPos    = CbusNeutral;  ///< current ComBus position after inertia filtering (ramp output).
    int8_t   driveState    = 0;            ///< FSM state index (0–4).
    uint16_t driveRampRate = 1u;           ///< Acceleration increment this cycle.
    uint16_t brakeRampRate = 1u;           ///< Braking increment this cycle.
    uint8_t  driveRampGain = 1u;           ///< Clutch-engagement multiplier (1, 2, or 4).
    uint32_t rampMillis    = 0u;           ///< Timestamp of last ramp step (millis()).
};



// =============================================================================
// 4. MOTION OUTPUT  (per-cycle result of the inertia pipeline)
// =============================================================================

/**
 * @brief Per-cycle result produced by the inertia pipeline.
 *
 * @details Consumed by the sound engine and the ESC output stage.
 *   All fields are valid for the current cycle only.
 */
struct MotionOutput {
    combus_t currentPos;   ///< current ComBus position after inertia filtering (ramp output).
    combus_t escCbusVal;   ///< Final ESC command (after scaling).
    combus_t currentSpeed; ///< Inertial deviation from neutral — used by sound engine.
    bool     isBraking;    ///< Pipeline is in a braking state.
    bool     inReverse;    ///< Pipeline is driving in reverse.
    bool     isDriving;    ///< Pipeline is actively driving (forward or reverse).
};

// EOF motion_struct.h
