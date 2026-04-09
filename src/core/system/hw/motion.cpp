/******************************************************************************
 * @file motion.cpp
 * @brief Stateless motion-control pipeline — implementation.
 *****************************************************************************/

#include "motion.h"

#include <cassert>

#include <core/system/debug/debug.h>    // sys_log_err


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
 * @brief Ramp processor. Move motion current position one step closer to raw ComBus value each ramp tick, and fill output.
 *
 * @details
 * Inertia principle : Motion currentPos never jumps to rawComBusVal directly.
 *   Each time the ramp period (`rampMs`) elapses, currentPos is moved by a fixed
 *   step size (accelSteps or brakeSteps) to approach rawComBusVal.
 *   This creates the "weight" effect: the faster the step (small rampMs) and the
 *   bigger the step size, the more responsive the motion.  Larger rampMs or smaller
 *   steps simulate a heavier / lazier machine.
 *
 * Two modes for the same skeleton, but different parameters:
 *
 * - Simple ramp (config->ramp != null):
 *     rampMs    = config->ramp->rampTimeMs — fixed, same for accel and brake.
 *     accelStep = config->ramp->accelSteps — fixed increment per tick.
 *     brakeStep = config->ramp->brakeSteps — fixed decrement per tick.
 *     No gear or inertia model.  Used for hydraulics and steering.
 *
 * - Traction (config->gear + config->inertia both != null):
 *     rampMs    = per-gear period (1st / 2nd / 3rd / direct), optionally scaled
 *                 by globalAccelPct (< 100 % = shorter period = faster response).
 *     accelStep = config->inertia->accelSteps * driveRampGain — gain is a
 *                 runtime multiplier (1, 2, or 4) simulating clutch engagement.
 *     brakeStep = config->inertia->brakeSteps — fixed; may be asymmetric to
 *                 simulate engine braking heavier than free decel.
 *     brakeMargin: the lowest ComBus value currentPos could reach during braking
 *                 (currentPos - brakeMargin).  Passed (future) to the sound engine
 *                 to gauge engine-braking intensity.  Not applied to currentPos itself.
 *
 * Dead-band: values of rawComBusVal within [minNeutral, maxNeutral] are snapped
 * to CbusNeutral before any ramp logic, preventing drift around stick center.
 *
 * isBraking: true in traction mode when currentPos is non-neutral AND moving
 * back toward neutral (i.e. target is neutral or target is closer to neutral than
 * current position).  Always false in simple ramp mode.
 *
 * Steps reference:
 *   1. Clamp rawComBusVal to travel limits, snap dead-band to neutral.
 *   2. Auto-advance virtual gear from currentPos (traction only; skipped when gearAdvance is false).
 *   3. Compute ramp period: gear table (traction) or config->ramp->rampTimeMs.
 *   4. Ramp tick: if period elapsed, shift currentPos by accel/brake step.
 *   5. Detect isBraking (traction only).
 *   6. Fill output struct.
 *
 * @param rawComBusVal  Raw ComBus value, unfiltered (straight from the receiver).
 * @param config        Active config — algorithm selected by pointer pattern.
 * @param runtime       Runtime state — currentPos and ramp timer updated in place.
 * @param output        Optional output struct filled with computed state; nullptr to skip.
 */

static void motion_process(combus_t            rawComBusVal,
                           const MotionConfig* config,
                           MotionRuntime*      runtime,
                           MotionOutput*       output)
{
    const combus_t maxLimit = s_maxLimit(config);
    const combus_t minLimit = s_minLimit(config);

      // 1. Clamp to travel limits, snap dead-band to neutral
    rawComBusVal = s_clamp(rawComBusVal, minLimit, maxLimit);
    if (rawComBusVal >= config->band->minNeutral && rawComBusVal <= config->band->maxNeutral) {
        rawComBusVal = CbusNeutral;
    }

      // 2. Auto-advance gear (traction only)
    if (config->gear) {
        const uint32_t halfTravel = (runtime->currentPos >= CbusNeutral)
                                  ? static_cast<uint32_t>(maxLimit - CbusNeutral)
                                  : static_cast<uint32_t>(CbusNeutral - minLimit);
        const uint32_t dev = (runtime->currentPos >= CbusNeutral)
                            ? static_cast<uint32_t>(runtime->currentPos - CbusNeutral)
                            : static_cast<uint32_t>(CbusNeutral - runtime->currentPos);
        if (halfTravel > 0u) {
            if      (dev * 3u >= halfTravel * 2u) runtime->gearSetTo = 3;
            else if (dev * 3u >= halfTravel)      runtime->gearSetTo = 2;
            else                                  runtime->gearSetTo = 1;
        }
    }

      // 3. Ramp period: per-gear table (traction) or fixed period (simple ramp)
    uint16_t rampMs;
    if (config->gear) {
        switch (runtime->gearSetTo) {
            case 1:  rampMs = config->gear->rampTimeFirstMs;   break;
            case 2:  rampMs = config->gear->rampTimeSecondMs; break;
            case 3:  rampMs = config->gear->rampTimeThirdMs;  break;
            default: rampMs = config->gear->rampTimeFirstMs;    break;
        }
          // Global acceleration scaling (100 % = no scaling)
        if (config->gear->globalAccelPct != 100u) {
            rampMs = static_cast<uint16_t>(static_cast<uint32_t>(rampMs) * config->gear->globalAccelPct / 100u);
            if (rampMs == 0u) rampMs = 1u;
        }
    } else {
        rampMs = config->ramp->rampTimeMs;
    }

      // 4. Ramp tick: if period elapsed, shift currentPos by accel/brake step.
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
                  // brakeMin: lowest ComBus value currentPos could reach during this brake phase —
                  //           passed to the sound engine to gauge engine-braking intensity. Not used yet.
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

    runtime->currentPos = s_clamp(runtime->currentPos, minLimit, maxLimit);

      // 5. isBraking: currentPos moving back toward neutral (traction only)
    const bool isBraking = config->inertia &&
                           (runtime->currentPos != CbusNeutral) &&
                           (rawComBusVal == CbusNeutral                                               ||
                            (runtime->currentPos > CbusNeutral && rawComBusVal < runtime->currentPos) ||
                            (runtime->currentPos < CbusNeutral && rawComBusVal > runtime->currentPos));

      // 6. Fill output struct
    if (output) {
        output->currentPos   = runtime->currentPos;
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
 *   1. Run motion_process(): clamp rawComBusVal, auto-advance gear, resolve ramp period, advance currentPos,
 *      compute isBraking, fill @p output.
 *   2. Write runtime->currentPos to the ComBus channel — caller's responsibility
 *      (use DcDevice::comChannel as the index into ComBus::analog[]).
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

      // 2. ComBus write is the caller's responsibility — see motion.h @note.
#endif // MOTION_ENABLED
}


// =============================================================================
// 5. GEAR CONTROL
// =============================================================================

/**
 * @brief Override the active virtual gear or disable gear advance.
 *
 * @details Overrides the automatic gear-advance inside `motion_process()`.
 *   Pass `gear = 0` to disable gear advance: `rampTimeDirectMs` is used, giving
 *   near-direct response.  Automatic advance resumes as soon as `gearSetTo != 0`
 *   on the next `motion_update()` call.
 *   Pass `gear = 1`–`3` to force a specific gear.
 *
 * @param runtime  Per-device runtime.  Must not be null.
 * @param gear     0 = gear advance disabled (direct), 1–3 = force gear.  Values > 3
 *                 fall through to `rampTimeDirectMs` inside `motion_process()`.
 */

void motion_gear_set(MotionRuntime* runtime, uint8_t gear)
{
#if !defined(MOTION_ENABLED)
    (void)runtime; (void)gear;
#else
    assert(runtime != nullptr);
    runtime->gearSetTo = static_cast<int8_t>(gear);
#endif // MOTION_ENABLED
}


// EOF motion.cpp
