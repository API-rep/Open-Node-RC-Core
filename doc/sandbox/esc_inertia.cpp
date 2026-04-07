// [SANDBOX — migration reference only, not compiled]
// Original: src/core/system/hw/esc_inertia.cpp
// Delete migrated sections as they are covered by the new motion architecture.

/******************************************************************************
 * @file esc_inertia.cpp
 * @brief Vehicle inertia FSM — stateless implementation.
 *****************************************************************************/


// =============================================================================
// 1. HELPERS  →  migrate into motion.cpp apply_* steps
// =============================================================================

// --- raw_pulse_dir()  →  motion_pulse_dir() (input side)
// Returns +1 forward / -1 backward / 0 neutral from raw ComBus value.
// Uses: cbusMaxNeutral, cbusMinNeutral, cbusMaxLimit, cbusMinLimit
static int8_t raw_pulse_dir(uint16_t pw, const EscInertiaConfig& c)
{
    if (pw > c.cbusMaxNeutral && pw < c.cbusMaxLimit) return  1;
    if (pw < c.cbusMinNeutral && pw > c.cbusMinLimit) return -1;
    return 0;
}

// --- esc_pulse_dir()  →  motion_pulse_dir() (inertial position side)
// Same logic applied to rt.cbusPos.  Can be merged with raw_pulse_dir().
static int8_t esc_pulse_dir(uint16_t pw, const EscInertiaConfig& c)
{
    if (pw > c.cbusMaxNeutral && pw < c.cbusMaxLimit) return  1;
    if (pw < c.cbusMinNeutral && pw > c.cbusMinLimit) return -1;
    return 0;
}

// --- compute_ramp_time()  →  MotionGear apply step
// Priority: overrideRampTimeMs > crawlerMode > automatic/DSG > gear 1/2/3+
// Then: × 100/globalAccelPct  ×  lowRangePct/100 (if lowRange)
static uint16_t compute_ramp_time(const EscInertiaInputs& inp,
                                  const EscInertiaConfig& c,
                                  const EscInertiaRuntime& rt,
                                  bool crawlerMode)
{
    if (inp.overrideRampTimeMs > 0u)
        return inp.overrideRampTimeMs;
    if (crawlerMode)
        return c.crawlerRampTimeMs;

    uint16_t rampMs;
    if (inp.automatic || inp.doubleClutch) {
        rampMs = c.rampTimeSecondMs;
        if (inp.automatic && esc_pulse_dir(rt.cbusPos, c) <= 0)
            rampMs = static_cast<uint16_t>(rampMs * 100u / c.autoRevAccelPct);
    } else {
        if      (inp.selectedGear <= 1) rampMs = c.rampTimeFirstMs;
        else if (inp.selectedGear == 2) rampMs = c.rampTimeSecondMs;
        else                            rampMs = c.rampTimeThirdMs;
    }
    rampMs = static_cast<uint16_t>(rampMs * 100u / c.globalAccelPct);
    if (inp.lowRange)
        rampMs = static_cast<uint16_t>(rampMs * c.lowRangePct / 100u);
    return rampMs;
}

// --- map_esc_signal()  →  MotionFsm apply step (linearize + scale)
// 1. Apply linearizeFn if non-null.
// 2. Scale [escMin..escMax] → [0..65535].
// Guard: if escMax <= escMin → return neutral safe (32767).
static uint16_t map_esc_signal(uint16_t pw, const EscInertiaConfig& c)
{
    uint16_t out = c.linearizeFn ? c.linearizeFn(pw) : pw;
    if (c.escMax <= c.escMin) return 32767u;
    int32_t mapped = static_cast<int32_t>(out - c.escMin) * 65535L
                   / static_cast<int32_t>(c.escMax - c.escMin);
    return static_cast<uint16_t>(
        mapped < 0L ? 0u : (mapped > 65535L ? 65535u : static_cast<uint16_t>(mapped))
    );
}


// =============================================================================
// 2. UPDATE  →  motion_update() in motion.cpp
// =============================================================================

void esc_inertia_update(uint16_t                cbusVal,
                        const EscInertiaInputs& inputs,
                        const EscInertiaConfig* cfg,
                        EscInertiaRuntime&      rt,
                        EscInertiaState*        out)
{
    // --- passthrough (cfg == nullptr)  →  Layer 0 default (no MotionConfig)
    if (!cfg) {
        if (out) {
            out->cbusPos         = cbusVal;
            out->escCbusVal      = cbusVal;
            out->currentSpeed    = 0u;
            out->escIsBraking    = false;
            out->escInReverse    = false;
            out->escIsDriving    = false;
            out->brakeDetect     = false;
            out->airBrakeTrigger = false;
        }
        return;
    }

    const EscInertiaConfig& c = *cfg;

    // =========================================================================
    // RAMP_SIMPLE path  →  MotionRamp apply step
    // =========================================================================
    if (c.mode == MotionMode::RAMP_SIMPLE) {
        if (millis() - rt.rampMillis > c.rampTimeFirstMs) {
            rt.rampMillis = millis();
            if (cbusVal > rt.cbusPos) {
                const uint32_t step = inputs.failSafe ? c.brakeSteps : c.accelSteps;
                const uint32_t next = static_cast<uint32_t>(rt.cbusPos) + step;
                rt.cbusPos = (next >= cbusVal) ? cbusVal
                           : static_cast<uint16_t>(next > 65535u ? 65535u : next);
            } else if (cbusVal < rt.cbusPos) {
                const uint32_t step = inputs.failSafe ? c.brakeSteps : c.accelSteps;
                rt.cbusPos = (rt.cbusPos <= step + cbusVal)
                           ? cbusVal
                           : static_cast<uint16_t>(rt.cbusPos - step);
            }
        }
        uint16_t escSignal = map_esc_signal(rt.cbusPos, c);
        if (c.comBus)
            combus_set_analog(*c.comBus, AnalogComBusID::ESC_SPEED_BUS, escSignal, c.owner);
        if (out) {
            out->cbusPos         = rt.cbusPos;
            out->escCbusVal      = escSignal;
            out->currentSpeed    = 0u;
            out->escIsBraking    = false;
            out->escInReverse    = (rt.cbusPos < c.cbusMinNeutral);
            out->escIsDriving    = (rt.cbusPos > c.cbusMaxNeutral || rt.cbusPos < c.cbusMinNeutral);
            out->brakeDetect     = false;
            out->airBrakeTrigger = false;
        }
        return;
    }

    // =========================================================================
    // TRACTION_FSM path  →  MotionFsm apply step (states 0–4)
    // =========================================================================

    bool crawlerMode = inputs.crawlerMode;
    uint16_t rampTime = compute_ramp_time(inputs, c, rt, crawlerMode);

    // --- brakeRampRate / driveRampRate: proportional to currentThrottle ---
    if (crawlerMode) {
        rt.brakeRampRate = static_cast<uint16_t>(
            static_cast<uint32_t>(c.accelSteps) * inputs.currentThrottle / 500u + 1u);
        rt.driveRampRate = c.accelSteps;
    } else {
        rt.brakeRampRate = static_cast<uint16_t>(
            static_cast<uint32_t>(c.brakeSteps) * inputs.currentThrottle / 500u + 1u);
        rt.driveRampRate = static_cast<uint16_t>(
            static_cast<uint32_t>(c.accelSteps) * inputs.currentThrottle / 500u + 1u);
    }
    if (inputs.failSafe) {
        rt.brakeRampRate = c.brakeSteps;
        rt.driveRampRate = c.brakeSteps;
    }

    int8_t pulseDir  = raw_pulse_dir(cbusVal, c);
    int8_t escDir    = esc_pulse_dir(rt.cbusPos, c);
    bool brakeDetect = (pulseDir ==  1 && escDir == -1)
                    || (pulseDir == -1 && escDir ==  1);

    bool escIsBraking    = false;
    bool escInReverse    = false;
    bool escIsDriving    = false;
    bool airBrakeTrigger = false;

    // --- FSM tick (gated by ramp timer) ---
    if (millis() - rt.rampMillis > rampTime) {
        rt.rampMillis = millis();

        switch (rt.driveState) {

        case 0: // Standing still
            escIsBraking = false; escInReverse = false; escIsDriving = false;
            rt.cbusPos = c.cbusNeutral;
            if (pulseDir ==  1 && inputs.engineRunning && !inputs.neutralGear) rt.driveState = 1;
            if (pulseDir == -1 && inputs.engineRunning && !inputs.neutralGear) rt.driveState = 3;
            break;

        case 1: // Driving forward
            escIsBraking = false; escInReverse = false; escIsDriving = true;
            if (rt.cbusPos < cbusVal && inputs.currentSpeed < inputs.speedLimit
                && !inputs.batteryProtection)
            {
                if (rt.cbusPos >= c.escMaxNeutral)
                    rt.cbusPos += static_cast<uint16_t>(
                        static_cast<uint32_t>(rt.driveRampRate) * rt.driveRampGain);
                else
                    rt.cbusPos = c.escMaxNeutral;
            }
            if ((rt.cbusPos > cbusVal || inputs.batteryProtection)
                && rt.cbusPos > c.cbusNeutral)
            {
                const uint16_t step = static_cast<uint16_t>(
                    static_cast<uint32_t>(rt.driveRampRate) * rt.driveRampGain);
                rt.cbusPos = (rt.cbusPos > c.cbusNeutral + step)
                    ? static_cast<uint16_t>(rt.cbusPos - step)
                    : c.cbusNeutral;
            }
            // gear shift sync: gearUp → proportional back-off; gearDown → small boost
            if (inputs.gearUpShiftingPulse && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                const uint16_t backOff = static_cast<uint16_t>(
                    static_cast<uint32_t>(inputs.currentSpeed) * 65535u / 4000u);
                rt.cbusPos = (rt.cbusPos > c.cbusNeutral + backOff)
                    ? static_cast<uint16_t>(rt.cbusPos - backOff) : c.cbusNeutral;
                if (rt.cbusPos > c.cbusMax) rt.cbusPos = c.cbusMax;
            }
            if (inputs.gearDownShiftingPulse && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                const uint16_t boost = static_cast<uint16_t>(50u * 65535u / 1000u);
                if (rt.cbusPos <= 65535u - boost) rt.cbusPos += boost;
                if (rt.cbusPos < c.cbusNeutral) rt.cbusPos = c.cbusNeutral;
                if (rt.cbusPos > c.cbusMax)     rt.cbusPos = c.cbusMax;
            }
            if (pulseDir == -1 && escDir ==  1) rt.driveState = 2;
            if (pulseDir == -1 && escDir ==  0) rt.driveState = 3;
            if (pulseDir ==  0 && escDir ==  0) rt.driveState = 0;
            break;

        case 2: // Braking forward
            escIsBraking = true; escInReverse = false; escIsDriving = false;
            if (rt.cbusPos > c.cbusNeutral) {
                rt.cbusPos = (rt.cbusPos > c.cbusNeutral + rt.brakeRampRate)
                    ? static_cast<uint16_t>(rt.cbusPos - rt.brakeRampRate)
                    : c.cbusNeutral;
            }
            if (rt.cbusPos < c.cbusNeutral + c.brakeMargin && pulseDir == -1)
                rt.cbusPos = static_cast<uint16_t>(c.cbusNeutral + c.brakeMargin);
            if (rt.cbusPos < c.cbusNeutral && pulseDir == 0)
                rt.cbusPos = c.cbusNeutral;
            if (pulseDir == 0 && escDir ==  1 && !inputs.neutralGear) { rt.driveState = 1; airBrakeTrigger = true; }
            if (pulseDir == 0 && escDir == 0)                          { rt.driveState = 0; airBrakeTrigger = true; }
            break;

        case 3: // Driving backward
            escIsBraking = false; escInReverse = true; escIsDriving = true;
            if (rt.cbusPos > cbusVal && inputs.currentSpeed < inputs.speedLimit
                && !inputs.batteryProtection)
            {
                if (rt.cbusPos <= c.escMinNeutral) {
                    const uint16_t step = static_cast<uint16_t>(
                        static_cast<uint32_t>(rt.driveRampRate) * rt.driveRampGain);
                    rt.cbusPos = (rt.cbusPos > step) ? static_cast<uint16_t>(rt.cbusPos - step) : 0u;
                } else {
                    rt.cbusPos = c.escMinNeutral;
                }
            }
            if ((rt.cbusPos < cbusVal || inputs.batteryProtection)
                && rt.cbusPos < c.cbusNeutral)
            {
                const uint16_t step = static_cast<uint16_t>(
                    static_cast<uint32_t>(rt.driveRampRate) * rt.driveRampGain);
                rt.cbusPos = (rt.cbusPos <= 65535u - step)
                    ? static_cast<uint16_t>(rt.cbusPos + step) : c.cbusNeutral;
            }
            if (inputs.gearUpShiftingPulse && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                const uint16_t backOff = static_cast<uint16_t>(
                    static_cast<uint32_t>(inputs.currentSpeed) * 65535u / 4000u);
                if (rt.cbusPos <= 65535u - backOff) rt.cbusPos += backOff;
                if (rt.cbusPos < c.cbusMin)     rt.cbusPos = c.cbusMin;
                if (rt.cbusPos > c.cbusNeutral) rt.cbusPos = c.cbusNeutral;
            }
            if (inputs.gearDownShiftingPulse && inputs.shiftingAutoThrottle
                && !inputs.automatic && !inputs.doubleClutch)
            {
                const uint16_t boost = static_cast<uint16_t>(50u * 65535u / 1000u);
                rt.cbusPos = (rt.cbusPos > boost) ? static_cast<uint16_t>(rt.cbusPos - boost) : 0u;
                if (rt.cbusPos < c.cbusMin)     rt.cbusPos = c.cbusMin;
                if (rt.cbusPos > c.cbusNeutral) rt.cbusPos = c.cbusNeutral;
            }
            if (pulseDir ==  1 && escDir == -1) rt.driveState = 4;
            if (pulseDir ==  1 && escDir ==  0) rt.driveState = 1;
            if (pulseDir ==  0 && escDir ==  0) rt.driveState = 0;
            break;

        case 4: // Braking backward
            escIsBraking = true; escInReverse = true; escIsDriving = false;
            if (rt.cbusPos < c.cbusNeutral) {
                const uint16_t step = rt.brakeRampRate;
                rt.cbusPos = (rt.cbusPos <= 65535u - step)
                    ? static_cast<uint16_t>(rt.cbusPos + step) : c.cbusNeutral;
            }
            if (rt.cbusPos > c.cbusNeutral - c.brakeMargin && pulseDir == 1)
                rt.cbusPos = static_cast<uint16_t>(c.cbusNeutral - c.brakeMargin);
            if (rt.cbusPos > c.cbusNeutral && pulseDir == 0)
                rt.cbusPos = c.cbusNeutral;
            if (pulseDir == 0 && escDir == -1 && !inputs.neutralGear) { rt.driveState = 3; airBrakeTrigger = true; }
            if (pulseDir == 0 && escDir == 0)                          { rt.driveState = 0; airBrakeTrigger = true; }
            break;

        } // end switch

        // --- clutch ramp gain  →  MotionFsm apply step
        // currentSpeed < 50: gain=2 (manual) or 4 (auto/DSG) — simulates clutch engagement
        // currentSpeed >= 50: gain=1 (cruising)
        if (inputs.currentSpeed < 50u) {
            rt.driveRampGain = (!inputs.automatic && !inputs.doubleClutch) ? 2 : 4;
        } else {
            rt.driveRampGain = 1;
        }
    }

    // --- output build (every call, not gated)  →  MotionState fill ---
    uint16_t escSignal = map_esc_signal(rt.cbusPos, c);

    // --- currentSpeed 0–500 from inertial position  →  MotionState fill
    uint16_t currentSpeed = 0u;
    if (rt.cbusPos > c.cbusMaxNeutral)
        currentSpeed = static_cast<uint16_t>(
            500L * static_cast<long>(rt.cbusPos - c.cbusMaxNeutral)
                 / static_cast<long>(c.cbusMax - c.cbusMaxNeutral));
    else if (rt.cbusPos < c.cbusMinNeutral)
        currentSpeed = static_cast<uint16_t>(
            500L * static_cast<long>(c.cbusMinNeutral - rt.cbusPos)
                 / static_cast<long>(c.cbusMinNeutral - c.cbusMin));

    // --- ComBus write  →  motion_update() tail
    if (c.comBus)
        combus_set_analog(*c.comBus, AnalogComBusID::ESC_SPEED_BUS, escSignal, c.owner);

    // --- fill output struct
    if (out) {
        out->cbusPos         = rt.cbusPos;
        out->escCbusVal      = escSignal;
        out->currentSpeed    = currentSpeed;
        out->escIsBraking    = escIsBraking;
        out->escInReverse    = escInReverse;
        out->escIsDriving    = escIsDriving;
        out->brakeDetect     = brakeDetect;
        out->airBrakeTrigger = airBrakeTrigger;
    }
}

// EOF sandbox/esc_inertia.cpp
