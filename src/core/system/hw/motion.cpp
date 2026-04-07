/******************************************************************************
 * @file motion.cpp
 * @brief Stateless motion-control pipeline — implementation.
 *****************************************************************************/

#include "motion.h"

#include <cassert>

#include <core/system/debug/debug.h>    // sys_log_err
#include <struct/combus_struct.h>       // ChanOwner


// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

/** @brief Upper travel limit. Returns software margin when configured, otherwise hw hard stop. */
static inline combus_t s_maxLimit(const MotionConfig* config) {
    return config->margin ? config->margin->maxVal : config->hw->maxHwVal;
}

/** @brief Lower travel limit. Returns software margin when configured, otherwise hw hard stop. */
static inline combus_t s_minLimit(const MotionConfig* config) {
    return config->margin ? config->margin->minVal : config->hw->minHwVal;
}

/** @brief Value clamper. Keep @p val within [@p low, @p high] to stay within travel limits. */
static inline combus_t s_clamp(combus_t val, combus_t low, combus_t high)
{
    if (val < low) return low;
    if (val > high) return high;
    return val;
}


// =============================================================================
// 2. CONFIG VALIDATION
// =============================================================================

/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Checks in order, stops on the first failure:
 *   1. hw and band pointer are non-null (mandatory for all modes).
 *   2. Algorithm coherence: ramp XOR (gear + inertia).
 *   3. hw->maxHwVal > hw->minHwVal.
 *   4. margin (when present) within hw limits, max > min.
 *   5. dead-band within the effective limits (margin or hw).
 */

bool motion_check(const MotionConfig* cfg)
{
#if !defined(MOTION_ENABLED)
    return true;
#else
    assert(cfg != nullptr);

      // 1. Mandatory pointers
    if (!cfg->hw || !cfg->band) {
        sys_log_err("motion_check: hw and band are mandatory");
        return false;
    }

      // 2. Algorithm coherence
    const bool hasRamp    = (cfg->ramp    != nullptr);
    const bool hasGear    = (cfg->gear    != nullptr);
    const bool hasInertia = (cfg->inertia != nullptr);

    if (hasRamp && (hasGear || hasInertia)) {
        sys_log_err("motion_check: ramp and gear/inertia are mutually exclusive");
        return false;
    }
    if (!hasRamp && !(hasGear && hasInertia)) {
        sys_log_err("motion_check: traction mode requires both gear and inertia");
        return false;
    }

      // 3. hw range
    if (cfg->hw->maxHwVal <= cfg->hw->minHwVal) {
        sys_log_err("motion_check: hw->maxHwVal must be > hw->minHwVal");
        return false;
    }

      // 4. margin within hw
    if (cfg->margin) {
        if (cfg->margin->maxVal > cfg->hw->maxHwVal ||
            cfg->margin->minVal < cfg->hw->minHwVal) {
            sys_log_err("motion_check: margin must be within hw limits");
            return false;
        }
        if (cfg->margin->maxVal <= cfg->margin->minVal) {
            sys_log_err("motion_check: margin->maxVal must be > minVal");
            return false;
        }
    }

      // 5. dead-band within travel limits
    const combus_t maxLimit = s_maxLimit(cfg);
    const combus_t minLimit = s_minLimit(cfg);

    if (cfg->band->maxNeutral < cfg->band->minNeutral) {
        sys_log_err("motion_check: band->maxNeutral must be >= minNeutral");
        return false;
    }
    if (cfg->band->maxNeutral > maxLimit || cfg->band->minNeutral < minLimit) {
        sys_log_err("motion_check: dead-band must be within effective limits");
        return false;
    }

    return true;
#endif // MOTION_ENABLED
}


// =============================================================================
// 3. PIPELINE UPDATE
// =============================================================================

#if defined(MOTION_ENABLED)

/**
 * @brief Ramp processor. Advance cbusPos one step toward rawComBusVal and fill output.
 *
 * @details Both modes share the same skeleton:
 *   1. Clamp rawComBusVal to effective limits, snap dead-band to neutral.
 *   2. Compute ramp period: gear table (traction) or config->ramp->rampTimeMs.
 *   3. If the period has elapsed, advance cbusPos by one accel or brake step.
 *      In traction mode driveRampGain scales the accel step.
 *      brakeMin is computed but not yet used — reserved for the sound engine.
 *   4. Detect isBraking: cbusPos retreating toward neutral (traction only).
 *   5. Write all fields to output (skipped when output == nullptr).
 *
 * @param rawComBusVal  Receiver value after dead-band snap, clamped to hw limits.
 * @param config        Active config — algorithm selected by pointer pattern.
 * @param runtime       Runtime state — cbusPos and ramp timer updated in place.
 * @param output        Optional output struct filled with computed state; nullptr to skip.
 */

static void motion_process(combus_t            rawComBusVal,
                           const MotionConfig* config,
                           MotionRuntime*      runtime,
                           MotionOutput*       output)
{
    const combus_t effMax = s_maxLimit(config);
    const combus_t effMin = s_minLimit(config);

      // 1. Clamp to travel limits, snap dead-band to neutral
    rawComBusVal = s_clamp(rawComBusVal, effMin, effMax);
    if (rawComBusVal >= config->band->minNeutral && rawComBusVal <= config->band->maxNeutral) {
        rawComBusVal = CbusNeutral;
    }

      // 2. Ramp period: per-gear table (traction) or fixed period (simple ramp)
    uint16_t rampMs;
    if (config->gear) {
        switch (runtime->driveState) {
            case 1:  rampMs = config->gear->rampTimeFirstMs;   break;
            case 2:  rampMs = config->gear->rampTimeSecondMs;  break;
            case 3:  rampMs = config->gear->rampTimeThirdMs;   break;
            default: rampMs = config->gear->rampTimeCrawlerMs; break;
        }
          // Global acceleration scaling (100 % = no scaling)
        if (config->gear->globalAccelPct != 100u) {
            rampMs = static_cast<uint16_t>(
                         static_cast<uint32_t>(rampMs) * config->gear->globalAccelPct / 100u);
            if (rampMs == 0u) rampMs = 1u;
        }
    } else {
        rampMs = config->ramp->rampTimeMs;
    }

      // 3. Advance cbusPos by one step when the ramp period has elapsed
    const uint32_t now = millis();
    if (now - runtime->rampMillis >= rampMs) {
        runtime->rampMillis = now;

        if (runtime->currentPos < rawComBusVal) {
             // Accelerating — driveRampGain scales the step in traction mode
            const uint16_t gain = config->inertia ? runtime->driveRampGain : 1u;
            const combus_t step = config->inertia
                                  ? static_cast<combus_t>(config->inertia->accelSteps * gain)
                                  : static_cast<combus_t>(config->ramp->accelSteps);
            runtime->currentPos = (rawComBusVal - runtime->currentPos > step)
                                ? runtime->currentPos + step
                                : rawComBusVal;

        } else if (runtime->currentPos > rawComBusVal) {
             // Braking
            const combus_t step = config->inertia
                                  ? static_cast<combus_t>(config->inertia->brakeSteps)
                                  : static_cast<combus_t>(config->ramp->brakeSteps);
            if (config->inertia) {
                 // brakeMin: engine-braking floor transmitted to sound engine — not clamped here
                const combus_t brakeMin = (runtime->currentPos > config->inertia->brakeMargin)
                                          ? runtime->currentPos - config->inertia->brakeMargin
                                          : CbusNeutral;
                (void)brakeMin;
            }
            runtime->currentPos = (runtime->currentPos - rawComBusVal > step)
                                ? runtime->currentPos - step
                                : rawComBusVal;
        }
    }

    runtime->currentPos = s_clamp(runtime->currentPos, effMin, effMax);

    // 4. isBraking: cbusPos moving back toward neutral (traction only)
    const bool isBraking = config->inertia &&
                           (runtime->currentPos != CbusNeutral) &&
                           (rawComBusVal == CbusNeutral                                          ||
                            (runtime->currentPos > CbusNeutral && rawComBusVal < runtime->currentPos)  ||
                            (runtime->currentPos < CbusNeutral && rawComBusVal > runtime->currentPos));

    // 5. Fill output struct
    if (output) {
        output->currentPos      = runtime->currentPos;
        output->escCbusVal   = runtime->currentPos;
        output->currentSpeed = (runtime->currentPos >= CbusNeutral)
                               ? runtime->currentPos - CbusNeutral
                               : CbusNeutral - runtime->currentPos;
        output->isBraking    = isBraking;
        output->inReverse    = (runtime->currentPos < CbusNeutral);
        output->isDriving    = (runtime->currentPos != CbusNeutral);
    }
}

#endif // MOTION_ENABLED


// =============================================================================
// 4. PUBLIC UPDATE ENTRY POINT
// =============================================================================

/**
 * @brief Motion pipeline entry point. Call each control cycle to process rawComBusVal.
 *
 * @details
 *   1. Run motion_process(): clamp rawComBusVal, resolve ramp period, advance cbusPos one step,
 *      compute isBraking, fill @p output.
 *   2. Write runtime->currentPos to config->comBus when owner == SYSTEM
 *      (channel index is a placeholder until A6).
 */
void motion_update(combus_t            rawComBusVal,
                   const MotionConfig* config,
                   MotionRuntime*      runtime,
                   MotionOutput*       output)
{
#if !defined(MOTION_ENABLED)
    (void)rawComBusVal; (void)config; (void)runtime; (void)output;
    return;
#else
    assert(config  != nullptr);
    assert(runtime != nullptr);

      // 1. Run ramp pipeline
    motion_process(rawComBusVal, config, runtime, output);

      // 2. Write to ComBus when this node owns the channel
    if (config->comBus && config->owner == ChanOwner::SYSTEM) {
        config->comBus->analog[0] = runtime->currentPos;   // channel index resolved at A6
    }
#endif // MOTION_ENABLED
}


// EOF motion.cpp
