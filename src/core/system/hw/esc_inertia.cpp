/******************************************************************************
 * @file esc_inertia.cpp
 * @brief Vehicle inertia FSM — implementation.
 *
 * @details The FSM mirrors the DiYGuy `void esc()` drive state machine
 *   extracted into core so it is usable by both machine and sound nodes.
 *   Single-TU dependencies (`3_ESC.h` plain vars, DiYGuy macros) are
 *   replaced by the `EscInertiaConfig` and `EscInertiaInputs` structs.
 *
 *   Drive states:
 *   - 0  Standing still
 *   - 1  Driving forward
 *   - 2  Braking forward
 *   - 3  Driving backward
 *   - 4  Braking backward
 *
 *   ComBus write: `ESC_SPEED_BUS` receives the inertial `escSignalUs` value
 *   at every ramp-timer cycle.  The ownership check in `combus_set_analog()`
 *   enforces that only the declared owner ever writes this channel.
 *****************************************************************************/

#include "esc_inertia.h"
#include <core/system/combus/combus_access.h>
#include <core/config/combus/combus_types.h>
#include <core/system/debug/debug.h>
#include <Arduino.h>


// =============================================================================
// 1. MODULE STATE
// =============================================================================

#ifdef ESC_INERTIA_ENABLED

static EscInertiaConfig s_cfg{};

static uint16_t s_escPulseWidth     = 1500u; ///< Current inertial position
static int8_t   s_driveState        = 0;     ///< FSM state (0–4)
static int8_t   s_driveRampRate     = 1;
static int8_t   s_brakeRampRate     = 1;
static int8_t   s_driveRampGain     = 1;
static unsigned long s_rampMillis   = 0;

#endif // ESC_INERTIA_ENABLED


// =============================================================================
// 2. HELPERS  (file scope, only compiled when ESC_INERTIA_ENABLED)
// =============================================================================

#ifdef ESC_INERTIA_ENABLED

/// Throttle direction from raw RC pulse vs neutral dead-band.
/// Returns  1 = forward,  -1 = backward,  0 = neutral.
static int8_t raw_pulse_dir(uint16_t pw, const EscInertiaConfig& c)
{
    if (pw > c.pulseMaxNeutral && pw < c.pulseMaxLimit) return  1;
    if (pw < c.pulseMinNeutral && pw > c.pulseMinLimit) return -1;
    return 0;
}

/// ESC direction from current inertial position vs neutral dead-band.
static int8_t esc_pulse_dir(uint16_t pw, const EscInertiaConfig& c)
{
    if (pw > c.pulseMaxNeutral && pw < c.pulseMaxLimit) return  1;
    if (pw < c.pulseMinNeutral && pw > c.pulseMinLimit) return -1;
    return 0;
}

/// Compute active ramp time in ms from inputs and config.
/// Returns the value to use for the ramp-timer guard.
static uint16_t compute_ramp_time(const EscInertiaInputs& inp,
                                  const EscInertiaConfig& c,
                                  bool crawlerMode)
{
    // --- Override path for virtual gear ratios (VIRTUAL_3_SPEED / 16-speed) ---
    if (inp.overrideRampTimeMs > 0u)
        return inp.overrideRampTimeMs;

    if (crawlerMode)
        return c.crawlerRampTimeMs;

    uint16_t rt;
    if (inp.automatic || inp.doubleClutch) {
        rt = c.rampTimeSecondMs;
        if (inp.automatic && esc_pulse_dir(s_escPulseWidth, c) <= 0)
            rt = static_cast<uint16_t>(rt * 100u / c.autoRevAccelPct);
    } else {
        if      (inp.selectedGear <= 1) rt = c.rampTimeFirstMs;
        else if (inp.selectedGear == 2) rt = c.rampTimeSecondMs;
        else                            rt = c.rampTimeThirdMs;
    }

    rt = static_cast<uint16_t>(rt * 100u / c.globalAccelPct);
    if (inp.lowRange)
        rt = static_cast<uint16_t>(rt * c.lowRangePct / 100u);

    return rt;
}

/// Map inertial escPulseWidth to 1000–2000 µs ESC signal.
/// Applies the optional linearization function, then scales to 1000–2000.
/// Returns the final signal (ESC_DIR inversion must be applied externally if needed).
static uint16_t map_esc_signal(uint16_t pw, const EscInertiaConfig& c)
{
    uint16_t out = c.linearizeFn ? c.linearizeFn(pw) : pw;
    // map from [escPulseMin .. escPulseMax] → [1000 .. 2000]
    if (c.escPulseMax <= c.escPulseMin) return 1500u; // guard against bad config
    int32_t mapped = 1000L
        + static_cast<int32_t>(out - c.escPulseMin) * 1000L
        / static_cast<int32_t>(c.escPulseMax - c.escPulseMin);
    return static_cast<uint16_t>(
        mapped < 1000L ? 1000u : (mapped > 2000L ? 2000u : static_cast<uint16_t>(mapped))
    );
}

#endif // ESC_INERTIA_ENABLED


// =============================================================================
// 3. INIT
// =============================================================================

#ifdef ESC_INERTIA_ENABLED

void esc_inertia_init(const EscInertiaConfig& cfg)
{
    s_cfg          = cfg;
    s_escPulseWidth = cfg.pulseZero;
    s_driveState   = 0;
}

#else

void esc_inertia_init(const EscInertiaConfig& /*cfg*/) {}

#endif // ESC_INERTIA_ENABLED


// =============================================================================
// 4. UPDATE
// =============================================================================

#ifdef ESC_INERTIA_ENABLED

/**
 * @brief Advance the 5-state inertia FSM.
 *
 * @details The ramp timer guard (`s_rampMillis`) ensures the state machine
 *   steps only once per `escRampTimeMs` milliseconds, regardless of how often
 *   it is called.  All outputs are still written every call so the caller
 *   always has fresh flag values.
 */
void esc_inertia_update(uint16_t rawPulseUs,
                        const EscInertiaInputs& inputs,
                        EscInertiaState* out)
{
    const EscInertiaConfig& c = s_cfg;

    bool crawlerMode = inputs.crawlerMode;

    // --- Ramp rate computation ---
    uint16_t rampTime = compute_ramp_time(inputs, c, crawlerMode);

    if (crawlerMode) {
        // Direct mode: no inertia, ramp rates follow throttle
        s_brakeRampRate = static_cast<int8_t>(
            10L * static_cast<long>(inputs.currentThrottle) / 500L + 1L);
        s_driveRampRate = 10;
    } else {
        s_brakeRampRate = static_cast<int8_t>(
            static_cast<long>(c.brakeSteps) * inputs.currentThrottle / 500L + 1L);
        s_driveRampRate = static_cast<int8_t>(
            static_cast<long>(c.accelSteps) * inputs.currentThrottle / 500L + 1L);
    }

    if (inputs.failSafe) {
        s_brakeRampRate = static_cast<int8_t>(c.brakeSteps);
        s_driveRampRate = static_cast<int8_t>(c.brakeSteps);
    }

    int8_t pulseDir = raw_pulse_dir(rawPulseUs, c);
    int8_t escDir   = esc_pulse_dir(s_escPulseWidth, c);
    bool brakeDetect = (pulseDir ==  1 && escDir == -1)
                    || (pulseDir == -1 && escDir ==  1);

    bool escIsBraking = false;
    bool escInReverse = false;
    bool escIsDriving = false;
    bool airBrakeTrigger = false;

    // --- State machine (steps on ramp timer) ---
    if (millis() - s_rampMillis > rampTime) {
        s_rampMillis = millis();

        switch (s_driveState) {

        case 0: // Standing still -----------------------------------------------
            escIsBraking = false;
            escInReverse = false;
            escIsDriving = false;
            s_escPulseWidth = c.pulseZero;
            if (pulseDir ==  1 && inputs.engineRunning && !inputs.neutralGear)
                s_driveState = 1;
            if (pulseDir == -1 && inputs.engineRunning && !inputs.neutralGear)
                s_driveState = 3;
            break;

        case 1: // Driving forward -----------------------------------------------
            escIsBraking = false;
            escInReverse = false;
            escIsDriving = true;
            if (s_escPulseWidth < rawPulseUs
                && inputs.currentSpeed < inputs.speedLimit
                && !inputs.batteryProtection)
            {
                if (s_escPulseWidth >= c.escPulseMaxNeutral)
                    s_escPulseWidth += static_cast<uint16_t>(s_driveRampRate * s_driveRampGain);
                else
                    s_escPulseWidth = c.escPulseMaxNeutral; // initial boost
            }
            if ((s_escPulseWidth > rawPulseUs || inputs.batteryProtection)
                && s_escPulseWidth > c.pulseZero)
                s_escPulseWidth -= static_cast<uint16_t>(s_driveRampRate * s_driveRampGain);

            // --- Gear shifting sync ---
            if (inputs.gearUpShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                s_escPulseWidth -= inputs.currentSpeed / 4u;
                // constrain to [pulseZero .. pulseMax]
                if (s_escPulseWidth < c.pulseZero) s_escPulseWidth = c.pulseZero;
                if (s_escPulseWidth > c.pulseMax)  s_escPulseWidth = c.pulseMax;
            }
            if (inputs.gearDownShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                s_escPulseWidth += 50u;
                if (s_escPulseWidth < c.pulseZero) s_escPulseWidth = c.pulseZero;
                if (s_escPulseWidth > c.pulseMax)  s_escPulseWidth = c.pulseMax;
            }

            if (pulseDir == -1 && escDir ==  1) s_driveState = 2;
            if (pulseDir == -1 && escDir ==  0) s_driveState = 3;
            if (pulseDir ==  0 && escDir ==  0) s_driveState = 0;
            break;

        case 2: // Braking forward -----------------------------------------------
            escIsBraking = true;
            escInReverse = false;
            escIsDriving = false;
            if (s_escPulseWidth > c.pulseZero)
                s_escPulseWidth -= static_cast<uint16_t>(s_brakeRampRate);
            if (s_escPulseWidth < c.pulseZero + c.brakeMargin && pulseDir == -1)
                s_escPulseWidth = c.pulseZero + c.brakeMargin;
            if (s_escPulseWidth < c.pulseZero && pulseDir == 0)
                s_escPulseWidth = c.pulseZero;

            if (pulseDir == 0 && escDir ==  1 && !inputs.neutralGear) {
                s_driveState = 1;
                airBrakeTrigger = true;
            }
            if (pulseDir == 0 && escDir == 0) {
                s_driveState = 0;
                airBrakeTrigger = true;
            }
            break;

        case 3: // Driving backward ----------------------------------------------
            escIsBraking = false;
            escInReverse = true;
            escIsDriving = true;
            if (s_escPulseWidth > rawPulseUs
                && inputs.currentSpeed < inputs.speedLimit
                && !inputs.batteryProtection)
            {
                if (s_escPulseWidth <= c.escPulseMinNeutral)
                    s_escPulseWidth -= static_cast<uint16_t>(s_driveRampRate * s_driveRampGain);
                else
                    s_escPulseWidth = c.escPulseMinNeutral;
            }
            if ((s_escPulseWidth < rawPulseUs || inputs.batteryProtection)
                && s_escPulseWidth < c.pulseZero)
                s_escPulseWidth += static_cast<uint16_t>(s_driveRampRate * s_driveRampGain);

            if (inputs.gearUpShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                s_escPulseWidth += inputs.currentSpeed / 4u;
                if (s_escPulseWidth < c.pulseMin)  s_escPulseWidth = c.pulseMin;
                if (s_escPulseWidth > c.pulseZero) s_escPulseWidth = c.pulseZero;
            }
            if (inputs.gearDownShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                if (s_escPulseWidth > 50u) s_escPulseWidth -= 50u;
                if (s_escPulseWidth < c.pulseMin)  s_escPulseWidth = c.pulseMin;
                if (s_escPulseWidth > c.pulseZero) s_escPulseWidth = c.pulseZero;
            }

            if (pulseDir ==  1 && escDir == -1) s_driveState = 4;
            if (pulseDir ==  1 && escDir ==  0) s_driveState = 1;
            if (pulseDir ==  0 && escDir ==  0) s_driveState = 0;
            break;

        case 4: // Braking backward ----------------------------------------------
            escIsBraking = true;
            escInReverse = true;
            escIsDriving = false;
            if (s_escPulseWidth < c.pulseZero)
                s_escPulseWidth += static_cast<uint16_t>(s_brakeRampRate);
            if (s_escPulseWidth > c.pulseZero - c.brakeMargin && pulseDir == 1)
                s_escPulseWidth = c.pulseZero - c.brakeMargin;
            if (s_escPulseWidth > c.pulseZero && pulseDir == 0)
                s_escPulseWidth = c.pulseZero;

            if (pulseDir == 0 && escDir == -1 && !inputs.neutralGear) {
                s_driveState = 3;
                airBrakeTrigger = true;
            }
            if (pulseDir == 0 && escDir == 0) {
                s_driveState = 0;
                airBrakeTrigger = true;
            }
            break;

        } // end switch


        // --- Clutch ramp gain ---
        if (inputs.currentSpeed < 50u) { // clutchEngagingPoint approximation
            s_driveRampGain = (!inputs.automatic && !inputs.doubleClutch) ? 2 : 4;
        } else {
            s_driveRampGain = 1;
        }
    }

    // --- Build output --- (every call, not gated by ramp timer)
    uint16_t escSignal = map_esc_signal(s_escPulseWidth, c);

    // --- currentSpeed from inertial position ---
    uint16_t currentSpeed = 0u;
    if (s_escPulseWidth > c.pulseMaxNeutral)
        currentSpeed = static_cast<uint16_t>(
            500L * static_cast<long>(s_escPulseWidth - c.pulseMaxNeutral)
                 / static_cast<long>(c.pulseMax - c.pulseMaxNeutral));
    else if (s_escPulseWidth < c.pulseMinNeutral)
        currentSpeed = static_cast<uint16_t>(
            500L * static_cast<long>(c.pulseMinNeutral - s_escPulseWidth)
                 / static_cast<long>(c.pulseMinNeutral - c.pulseMin));

    // --- ComBus write: ESC_SPEED_BUS ---
    if (c.bus) {
        // Encode the inertial signal (1000–2000 µs) directly as the channel value.
        // The receiving node (sound node) uses it the same way it reads pulseWidth[3].
        combus_set_analog(*c.bus, AnalogComBusID::ESC_SPEED_BUS, escSignal, c.owner);
    }

    // --- Fill output struct ---
    if (out) {
        out->escPulseWidth  = s_escPulseWidth;
        out->escSignalUs    = escSignal;
        out->currentSpeed   = currentSpeed;
        out->escIsBraking   = escIsBraking;
        out->escInReverse   = escInReverse;
        out->escIsDriving   = escIsDriving;
        out->brakeDetect    = brakeDetect;
        out->airBrakeTrigger = airBrakeTrigger;
    }
}

#else

void esc_inertia_update(uint16_t /*rawPulseUs*/,
                        const EscInertiaInputs& /*inputs*/,
                        EscInertiaState* /*out*/) {}

#endif // ESC_INERTIA_ENABLED

// EOF esc_inertia.cpp
