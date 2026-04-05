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
 *   ComBus write: `ESC_SPEED_BUS` receives the inertial ESC signal (16-bit,
 *   0–65535) at every ramp-timer cycle.
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

static uint16_t  s_escPulseWidth     = 32767u; ///< Current inertial position (ComBus units)
static int8_t    s_driveState        = 0;      ///< FSM state (0–4)
static uint16_t  s_driveRampRate     = 1u;
static uint16_t  s_brakeRampRate     = 1u;
static uint8_t   s_driveRampGain     = 1u;
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
    if (pw > c.cbusMaxNeutral && pw < c.cbusMaxLimit) return  1;
    if (pw < c.cbusMinNeutral && pw > c.cbusMinLimit) return -1;
    return 0;
}

/// ESC direction from current inertial position vs neutral dead-band.
static int8_t esc_pulse_dir(uint16_t pw, const EscInertiaConfig& c)
{
    if (pw > c.cbusMaxNeutral && pw < c.cbusMaxLimit) return  1;
    if (pw < c.cbusMinNeutral && pw > c.cbusMinLimit) return -1;
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

/// Map inertial position to 16-bit ComBus ESC command.
/// Applies the optional linearization function, then scales [escMin..escMax] → [0..65535].
static uint16_t map_esc_signal(uint16_t pw, const EscInertiaConfig& c)
{
    uint16_t out = c.linearizeFn ? c.linearizeFn(pw) : pw;
    // map [escMin .. escMax] → [0 .. 65535]
    if (c.escMax <= c.escMin) return 32767u; // guard against bad config
    int32_t mapped = static_cast<int32_t>(out - c.escMin) * 65535L
                   / static_cast<int32_t>(c.escMax - c.escMin);
    return static_cast<uint16_t>(
        mapped < 0L ? 0u : (mapped > 65535L ? 65535u : static_cast<uint16_t>(mapped))
    );
}

#endif // ESC_INERTIA_ENABLED


// =============================================================================
// 3. INIT
// =============================================================================

#ifdef ESC_INERTIA_ENABLED

void esc_inertia_init(const EscInertiaConfig& cfg)
{
    s_cfg           = cfg;
    s_escPulseWidth = cfg.cbusNeutral;
    s_driveState    = 0;
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
void esc_inertia_update(uint16_t cbusVal,
                        const EscInertiaInputs& inputs,
                        EscInertiaState* out)
{
    const EscInertiaConfig& c = s_cfg;

    bool crawlerMode = inputs.crawlerMode;

    // --- Ramp rate computation ---
    uint16_t rampTime = compute_ramp_time(inputs, c, crawlerMode);

    if (crawlerMode) {
        // Direct mode: ramp rates scale with current throttle, max = accelSteps
        s_brakeRampRate = static_cast<uint16_t>(
            static_cast<uint32_t>(c.accelSteps) * inputs.currentThrottle / 500u + 1u);
        s_driveRampRate = c.accelSteps;
    } else {
        s_brakeRampRate = static_cast<uint16_t>(
            static_cast<uint32_t>(c.brakeSteps) * inputs.currentThrottle / 500u + 1u);
        s_driveRampRate = static_cast<uint16_t>(
            static_cast<uint32_t>(c.accelSteps) * inputs.currentThrottle / 500u + 1u);
    }

    if (inputs.failSafe) {
        s_brakeRampRate = c.brakeSteps;
        s_driveRampRate = c.brakeSteps;
    }

    int8_t pulseDir = raw_pulse_dir(cbusVal, c);
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
            s_escPulseWidth = c.cbusNeutral;
            if (pulseDir ==  1 && inputs.engineRunning && !inputs.neutralGear)
                s_driveState = 1;
            if (pulseDir == -1 && inputs.engineRunning && !inputs.neutralGear)
                s_driveState = 3;
            break;

        case 1: // Driving forward -----------------------------------------------
            escIsBraking = false;
            escInReverse = false;
            escIsDriving = true;
            if (s_escPulseWidth < cbusVal
                && inputs.currentSpeed < inputs.speedLimit
                && !inputs.batteryProtection)
            {
                if (s_escPulseWidth >= c.escMaxNeutral)
                    s_escPulseWidth += static_cast<uint16_t>(
                        static_cast<uint32_t>(s_driveRampRate) * s_driveRampGain);
                else
                    s_escPulseWidth = c.escMaxNeutral; // initial boost
            }
            if ((s_escPulseWidth > cbusVal || inputs.batteryProtection)
                && s_escPulseWidth > c.cbusNeutral)
            {
                const uint16_t step = static_cast<uint16_t>(
                    static_cast<uint32_t>(s_driveRampRate) * s_driveRampGain);
                s_escPulseWidth = (s_escPulseWidth > c.cbusNeutral + step)
                    ? static_cast<uint16_t>(s_escPulseWidth - step)
                    : c.cbusNeutral;
            }

            // --- Gear shifting sync ---
            if (inputs.gearUpShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                // proportional back-off: ~currentSpeed/4 µs equivalent scaled to 16-bit
                const uint16_t backOff = static_cast<uint16_t>(
                    static_cast<uint32_t>(inputs.currentSpeed) * 65535u / 4000u);
                s_escPulseWidth = (s_escPulseWidth > c.cbusNeutral + backOff)
                    ? static_cast<uint16_t>(s_escPulseWidth - backOff)
                    : c.cbusNeutral;
                if (s_escPulseWidth > c.cbusMax) s_escPulseWidth = c.cbusMax;
            }
            if (inputs.gearDownShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                // small forward boost: ~50 µs equivalent = 3277 ComBus units
                const uint16_t boost = static_cast<uint16_t>(50u * 65535u / 1000u);
                if (s_escPulseWidth <= 65535u - boost)
                    s_escPulseWidth += boost;
                if (s_escPulseWidth < c.cbusNeutral) s_escPulseWidth = c.cbusNeutral;
                if (s_escPulseWidth > c.cbusMax)     s_escPulseWidth = c.cbusMax;
            }

            if (pulseDir == -1 && escDir ==  1) s_driveState = 2;
            if (pulseDir == -1 && escDir ==  0) s_driveState = 3;
            if (pulseDir ==  0 && escDir ==  0) s_driveState = 0;
            break;

        case 2: // Braking forward -----------------------------------------------
            escIsBraking = true;
            escInReverse = false;
            escIsDriving = false;
            if (s_escPulseWidth > c.cbusNeutral)
            {
                s_escPulseWidth = (s_escPulseWidth > c.cbusNeutral + s_brakeRampRate)
                    ? static_cast<uint16_t>(s_escPulseWidth - s_brakeRampRate)
                    : c.cbusNeutral;
            }
            if (s_escPulseWidth < c.cbusNeutral + c.brakeMargin && pulseDir == -1)
                s_escPulseWidth = static_cast<uint16_t>(c.cbusNeutral + c.brakeMargin);
            if (s_escPulseWidth < c.cbusNeutral && pulseDir == 0)
                s_escPulseWidth = c.cbusNeutral;

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
            if (s_escPulseWidth > cbusVal
                && inputs.currentSpeed < inputs.speedLimit
                && !inputs.batteryProtection)
            {
                if (s_escPulseWidth <= c.escMinNeutral)
                {
                    const uint16_t step = static_cast<uint16_t>(
                        static_cast<uint32_t>(s_driveRampRate) * s_driveRampGain);
                    s_escPulseWidth = (s_escPulseWidth > step)
                        ? static_cast<uint16_t>(s_escPulseWidth - step)
                        : 0u;
                }
                else
                    s_escPulseWidth = c.escMinNeutral;
            }
            if ((s_escPulseWidth < cbusVal || inputs.batteryProtection)
                && s_escPulseWidth < c.cbusNeutral)
            {
                const uint16_t step = static_cast<uint16_t>(
                    static_cast<uint32_t>(s_driveRampRate) * s_driveRampGain);
                s_escPulseWidth = (s_escPulseWidth <= 65535u - step)
                    ? static_cast<uint16_t>(s_escPulseWidth + step)
                    : c.cbusNeutral;
            }

            if (inputs.gearUpShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                const uint16_t backOff = static_cast<uint16_t>(
                    static_cast<uint32_t>(inputs.currentSpeed) * 65535u / 4000u);
                if (s_escPulseWidth <= 65535u - backOff)
                    s_escPulseWidth += backOff;
                if (s_escPulseWidth < c.cbusMin)     s_escPulseWidth = c.cbusMin;
                if (s_escPulseWidth > c.cbusNeutral) s_escPulseWidth = c.cbusNeutral;
            }
            if (inputs.gearDownShiftingPulse
                && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                const uint16_t boost = static_cast<uint16_t>(50u * 65535u / 1000u);
                s_escPulseWidth = (s_escPulseWidth > boost)
                    ? static_cast<uint16_t>(s_escPulseWidth - boost)
                    : 0u;
                if (s_escPulseWidth < c.cbusMin)     s_escPulseWidth = c.cbusMin;
                if (s_escPulseWidth > c.cbusNeutral) s_escPulseWidth = c.cbusNeutral;
            }

            if (pulseDir ==  1 && escDir == -1) s_driveState = 4;
            if (pulseDir ==  1 && escDir ==  0) s_driveState = 1;
            if (pulseDir ==  0 && escDir ==  0) s_driveState = 0;
            break;

        case 4: // Braking backward ----------------------------------------------
            escIsBraking = true;
            escInReverse = true;
            escIsDriving = false;
            if (s_escPulseWidth < c.cbusNeutral)
            {
                const uint16_t step = s_brakeRampRate;
                s_escPulseWidth = (s_escPulseWidth <= 65535u - step)
                    ? static_cast<uint16_t>(s_escPulseWidth + step)
                    : c.cbusNeutral;
            }
            if (s_escPulseWidth > c.cbusNeutral - c.brakeMargin && pulseDir == 1)
                s_escPulseWidth = static_cast<uint16_t>(c.cbusNeutral - c.brakeMargin);
            if (s_escPulseWidth > c.cbusNeutral && pulseDir == 0)
                s_escPulseWidth = c.cbusNeutral;

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

    // --- currentSpeed from inertial position (0–500) ---
    uint16_t currentSpeed = 0u;
    if (s_escPulseWidth > c.cbusMaxNeutral)
        currentSpeed = static_cast<uint16_t>(
            500L * static_cast<long>(s_escPulseWidth - c.cbusMaxNeutral)
                 / static_cast<long>(c.cbusMax - c.cbusMaxNeutral));
    else if (s_escPulseWidth < c.cbusMinNeutral)
        currentSpeed = static_cast<uint16_t>(
            500L * static_cast<long>(c.cbusMinNeutral - s_escPulseWidth)
                 / static_cast<long>(c.cbusMinNeutral - c.cbusMin));

    // --- ComBus write: ESC_SPEED_BUS ---
    if (c.bus) {
        combus_set_analog(*c.bus, AnalogComBusID::ESC_SPEED_BUS, escSignal, c.owner);
    }

    // --- Fill output struct ---
    if (out) {
        out->cbusPos        = s_escPulseWidth;
        out->escCbusVal     = escSignal;
        out->currentSpeed   = currentSpeed;
        out->escIsBraking   = escIsBraking;
        out->escInReverse   = escInReverse;
        out->escIsDriving   = escIsDriving;
        out->brakeDetect    = brakeDetect;
        out->airBrakeTrigger = airBrakeTrigger;
    }
}

#else

void esc_inertia_update(uint16_t /*cbusVal*/,
                        const EscInertiaInputs& /*inputs*/,
                        EscInertiaState* /*out*/) {}

#endif // ESC_INERTIA_ENABLED

// EOF esc_inertia.cpp
