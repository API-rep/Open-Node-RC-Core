/******************************************************************************
 * @file esc_inertia.h
 * @brief Vehicle inertia FSM for ESC output — core, environment-agnostic.
 *
 * @details Implements the 5-state drive state machine (standing, forward,
 *   braking-forward, reverse, braking-reverse) that simulates vehicle mass
 *   by ramping the ESC pulse-width over time rather than tracking the
 *   throttle stick position directly.
 *
 *   **Ownership model:**
 *   The FSM writes `AnalogComBusID::ESC_SPEED_BUS` with an ownership tag
 *   declared in `EscInertiaConfig::owner`:
 *   - `ChanOwner::MACHINE_CTRL` — machine controller node runs the FSM,
 *     the sound node reads `ESC_SPEED_BUS` passively.
 *   - `ChanOwner::SOUND`        — transitional mode while the DiYGuy esc()
 *     FSM still lives in `sound_module/main.cpp` (single-TU constraint).
 *     Remove once the migration is complete.
 *
 *   **Usage:**
 *   @code
 *     // At init:
 *     EscInertiaConfig cfg{ ... };
 *     esc_inertia_init(cfg);
 *
 *     // In main loop / Task1:
 *     EscInertiaInputs inputs{ .engineRunning = true, .selectedGear = 2, ... };
 *     EscInertiaState  state{};
 *     esc_inertia_update(rawPulseUs, inputs, &state);
 *     if (state.airBrakeTrigger) trigger_air_brake_sound();
 *   @endcode
 *
 *   **Activation:** `-D ESC_INERTIA_ENABLED` in build flags.
 *   All public functions are no-ops when the flag is absent.
 *****************************************************************************/
#pragma once

#include <cstdint>
#include "esc.h"                            // EscLinearizeFn
#include <struct/combus_struct.h>           // ChanOwner, ComBus


// =============================================================================
// 1. CONFIGURATION  (set once at init, treated as const afterwards)
// =============================================================================

/**
 * @brief Vehicle-specific and board-specific parameters for the inertia FSM.
 *
 * @details Replaces the `3_ESC.h` plain C++ variables that previously locked
 *   the FSM into the single DiYGuy translation unit.  All fields map directly
 *   to existing `3_ESC.h` or `config.h §5` constants, easing migration.
 */
struct EscInertiaConfig {

    // --- Pulse-range calibration (from esc_calibrate()) ---
    uint16_t pulseZero;          ///< RC neutral = 1500 µs
    uint16_t pulseMax;           ///< RC full-forward limit (e.g. 2000)
    uint16_t pulseMin;           ///< RC full-reverse limit (e.g. 1000)
    uint16_t pulseMaxNeutral;    ///< Upper edge of neutral dead-band
    uint16_t pulseMinNeutral;    ///< Lower edge of neutral dead-band
    uint16_t pulseMaxLimit;      ///< Sanity cap — signal above this is invalid
    uint16_t pulseMinLimit;      ///< Sanity floor — signal below this is invalid

    // --- ESC hardware range (from esc_calibrate()) ---
    uint16_t escPulseMax;        ///< ← EscPulseSpan forward limit
    uint16_t escPulseMin;        ///< ← EscPulseSpan+EscReversePlus reverse limit
    uint16_t escPulseMaxNeutral; ///< ← EscTakeoffPunch dead-band positive edge
    uint16_t escPulseMinNeutral; ///< ← EscTakeoffPunch dead-band negative edge

    // --- Ramp timing (see 3_ESC.h escRampTimeFirstGear etc.) ---
    uint16_t rampTimeFirstMs;    ///< ms per ramp step in 1st gear (~20)
    uint16_t rampTimeSecondMs;   ///< ms per ramp step in 2nd gear (~50)
    uint16_t rampTimeThirdMs;    ///< ms per ramp step in 3rd gear (~75)
    uint16_t crawlerRampTimeMs;  ///< ms per step in crawler direct mode (~2)

    // --- Ramp rate limits ---
    uint8_t  brakeSteps;         ///< Max braking ramp steps (escBrakeSteps)
    uint8_t  accelSteps;         ///< Max acceleration ramp steps (escAccelerationSteps)
    uint8_t  brakeMargin;        ///< Min offset from neutral while braking (EscBrakeMargin)

    // --- Scaling percentages ---
    uint8_t  globalAccelPct;     ///< Global accel scaling 1–200 (100 = nominal)
    uint8_t  lowRangePct;        ///< Low-range ramp time delta in % (e.g. 50 → half speed)
    uint8_t  autoRevAccelPct;    ///< Automatic reverse accel boost (e.g. 200 = 2× faster)

    // --- Optional ESC hardware linearization ---
    /// Apply before mapping escPulseWidthOut → 1000–2000 signal.
    /// nullptr = passthrough (linear).  Use reMap(curveQuicrunFusion, ...) wrapper.
    EscLinearizeFn linearizeFn;

    // --- ComBus output ---
    ComBus*   bus;               ///< Target ComBus — writes ESC_SPEED_BUS. nullptr = skip.
    ChanOwner owner;             ///< MACHINE_CTRL (controller) or SOUND (transitional).
};


// =============================================================================
// 2. INPUTS  (filled by caller each loop cycle)
// =============================================================================

/**
 * @brief Per-cycle dynamic inputs for the inertia FSM.
 *
 * @details The caller (machine loop or transitional DiYGuy main.cpp) fills
 *   this struct from its own state variables and passes it to
 *   `esc_inertia_update()`.  Keeping inputs explicitly named avoids hidden
 *   global dependencies and makes the DiYGuy → core migration traceable.
 *
 *   `overrideRampTimeMs` is provided for virtual-gear-ratio configurations
 *   (VIRTUAL_3_SPEED, VIRTUAL_16_SPEED_SEQUENTIAL) where the caller pre-
 *   computes a gear-ratio-scaled ramp time.  Set to 0 for normal 3-gear use.
 */
struct EscInertiaInputs {
    bool     engineRunning;
    bool     neutralGear;
    bool     failSafe;                ///< RC failsafe active — force deceleration
    bool     batteryProtection;       ///< Low battery — disable acceleration
    bool     crawlerMode;             ///< Direct control mode (no virtual inertia)
    bool     lowRange;                ///< Low range transfer case engaged
    uint8_t  selectedGear;            ///< Active gear, 1-based (manual / sequential)
    bool     automatic;               ///< Automatic transmission active
    bool     doubleClutch;            ///< Double-clutch (DSG) transmission active
    bool     shiftingAutoThrottle;    ///< Auto-throttle during shifting enabled
    bool     gearUpShiftingPulse;     ///< One-shot pulse: shift-up just detected
    bool     gearDownShiftingPulse;   ///< One-shot pulse: shift-down just detected
    bool     gearUpShiftingInProgress;
    bool     gearDownShiftingInProgress;
    uint16_t currentThrottle;         ///< 0–500 from ENGINE_RPM_BUS (mapThrottle result)
    uint16_t speedLimit;              ///< 0–500 top speed (from virtual gear ratio)
    uint16_t overrideRampTimeMs;      ///< 0 = auto from selectedGear; non-zero = use directly
};


// =============================================================================
// 3. STATE  (output — filled by esc_inertia_update())
// =============================================================================

/**
 * @brief Per-cycle output state produced by the inertia FSM.
 *
 * @details Values here are consumed by the sound engine (`engineMassSimulation`,
 *   `mapThrottle`) and by the ESC output stage.  `airBrakeTrigger` is a
 *   one-shot pulse — the caller must process it and clear it on the same cycle.
 */
struct EscInertiaState {
    uint16_t escPulseWidth;    ///< Current inertial ESC position (1000–2000 µs, pre-linearize)
    uint16_t escSignalUs;      ///< Mapped 1000–2000 µs ESC command (post-linearize + ESC_DIR)
    uint16_t currentSpeed;     ///< 0–500 proportional to inertial deviation from neutral
    bool     escIsBraking;
    bool     escInReverse;
    bool     escIsDriving;
    bool     brakeDetect;
    bool     airBrakeTrigger;  ///< One-shot: pulsed true on the cycle that stops inertial motion
};


// =============================================================================
// 4. API
// =============================================================================

/// Initialise the inertia FSM with configuration.  Must be called once before
/// any `esc_inertia_update()` call.  No-op when ESC_INERTIA_ENABLED is absent.
void esc_inertia_init(const EscInertiaConfig& cfg);

/**
 * @brief Advance the inertia FSM by one cycle.
 *
 * @details Reads `rawPulseUs` (the current `ENGINE_RPM_BUS` stick value,
 *   1000–2000 µs), advances the 5-state drive state machine, updates
 *   `out->escPulseWidth`, writes `ESC_SPEED_BUS` into the ComBus declared
 *   in `EscInertiaConfig::bus`, and fills all `EscInertiaState` fields.
 *
 *   Call frequency is gated internally by `escRampTimeMs` — the function
 *   is safe to call every loop cycle and skips the state update when the
 *   ramp timer has not elapsed.
 *
 * @param rawPulseUs  Current throttle stick position in µs (ENGINE_RPM_BUS value).
 * @param inputs      Dynamic per-cycle inputs filled by the caller.
 * @param[out] out    Output state — may be nullptr if caller does not need it.
 *
 * @note No-op when ESC_INERTIA_ENABLED is absent.
 */
void esc_inertia_update(uint16_t rawPulseUs,
                        const EscInertiaInputs& inputs,
                        EscInertiaState* out);

// EOF esc_inertia.h
