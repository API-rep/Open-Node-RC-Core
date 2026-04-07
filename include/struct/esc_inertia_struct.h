/******************************************************************************
 * @file esc_inertia_struct.h
 * @brief Persistent data types for the vehicle-inertia motion controller.
 *
 * @details Groups the three structs whose instances outlive a single
 *   function call and must be visible at the `include/struct/` layer:
 *   `EscInertiaConfig` (set-once parameters), `EscInertiaRuntime` (per-
 *   instance mutable state), and `EscInertiaState` (per-cycle output
 *   snapshot consumed by sound and telemetry).
 *
 *   `EscLinearizeFn` and `MotionMode` are co-located here because both
 *   are embedded in `EscInertiaConfig` fields.
 *
 *   This header is included by `machines_struct.h` (via the `include/struct/`
 *   layer) so that `DcDevice` can embed `EscInertiaRuntime` and hold a
 *   `const EscInertiaConfig*` pointer inline.  The FSM implementation
 *   and all API functions live in `src/core/system/hw/esc_inertia.h/.cpp`.
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <struct/combus_struct.h>    // ChanOwner, ComBus


// =============================================================================
// 1. MOTION MODE
// =============================================================================

/**
 * @brief Selects the motion-control algorithm applied by the inertia FSM.
 *
 * @details The discriminator stored in `EscInertiaConfig::mode` selects the
 *   update path inside `esc_inertia_update()`.
 *
 *   `RAMP_SIMPLE`  — symmetric slew-rate limiting; follows the ComBus target
 *   as fast as the configured step rates allow, with no braking-state logic.
 *   Suited for hydraulic cylinders, steering actuators — anything that does
 *   not need virtual vehicle-mass simulation.
 *
 *   `TRACTION_FSM` — full 5-state drive/brake state machine (standing,
 *   driving-forward, braking-forward, driving-backward, braking-backward).
 *   Models vehicle mass and engine coupling; required for wheel/track traction.
 */
enum class MotionMode : uint8_t {
    RAMP_SIMPLE   = 0,  ///< Slew-rate only — hydraulics, steering actuators
    TRACTION_FSM  = 1,  ///< 5-state drive FSM — wheel or track traction
};


// =============================================================================
// 2. LINEARIZATION HOOK
// =============================================================================

/**
 * @brief Optional ESC output-curve correction callback.
 *
 * @details Applied inside `esc_inertia_update()` before the final
 *   `[escMin..escMax] → [0..65535]` scaling step.
 *
 *   `nullptr`      → passthrough (linear ESC, no correction).
 *   non-`nullptr`  → function is called with the raw inertial ComBus position
 *                    and must return the corrected value before scaling.
 *
 *   Typical use: `reMap(curveQuicrunFusion, val)` wrapper that indexes a
 *   pre-built look-up table to compensate for non-linear motor response.
 *
 * @param val  Raw inertial position in ComBus units (0–65535).
 * @return     Corrected position in ComBus units (0–65535).
 */
using EscLinearizeFn = uint16_t (*)(uint16_t val);


// =============================================================================
// 3. CONFIGURATION  (set once at init, treated as const afterwards)
// =============================================================================

/**
 * @brief Vehicle-specific and board-specific parameters for the inertia FSM.
 *
 * @details Instances are typically declared `constexpr` in machine config
 *   headers and their addresses stored in `DcDevice::motion`.  A `nullptr`
 *   pointer in `DcDevice::motion` means no inertia is applied for that device.
 *
 *   The `mode` field selects the control algorithm at runtime.  `RAMP_SIMPLE`
 *   ignores all gear, clutch, and engine-coupling fields; `TRACTION_FSM` uses
 *   all fields.  This lets the same struct type serve both wheel motors and
 *   hydraulic actuators without duplication.
 *
 *   All range fields (`cbusNeutral`, `cbusMax` … `escMinNeutral`) are expressed
 *   in 16-bit ComBus units (0–65535).  Derive them from `esc_calibrate()`.
 */
struct EscInertiaConfig {

    MotionMode mode = MotionMode::RAMP_SIMPLE;  ///< Algorithm selector — see MotionMode.

    // --- RC input range (16-bit ComBus units, from esc_calibrate()) ---
    uint16_t cbusNeutral;        ///< Neutral = 32767 (maps to 1500 µs)
    uint16_t cbusMax;            ///< Full-forward limit (e.g. 65535)
    uint16_t cbusMin;            ///< Full-reverse limit (e.g. 0)
    uint16_t cbusMaxNeutral;     ///< Upper edge of neutral dead-band
    uint16_t cbusMinNeutral;     ///< Lower edge of neutral dead-band
    uint16_t cbusMaxLimit;       ///< Sanity cap — values above are invalid
    uint16_t cbusMinLimit;       ///< Sanity floor — values below are invalid

    // --- ESC hardware range (16-bit ComBus units, from esc_calibrate()) ---
    uint16_t escMax;             ///< Forward full-travel limit
    uint16_t escMin;             ///< Reverse full-travel limit
    uint16_t escMaxNeutral;      ///< Positive-side takeoff dead-band edge
    uint16_t escMinNeutral;      ///< Negative-side takeoff dead-band edge

    // --- Ramp timing (ms per ramp step) ---
    uint16_t rampTimeFirstMs;    ///< 1st gear / slow ramp (~20 ms)
    uint16_t rampTimeSecondMs;   ///< 2nd gear / medium ramp (~50 ms)
    uint16_t rampTimeThirdMs;    ///< 3rd gear / fast ramp (~75 ms)
    uint16_t crawlerRampTimeMs;  ///< Direct/crawler mode ramp (~2 ms)

    // --- Ramp increments (ComBus units per step) ---
    uint16_t brakeSteps;         ///< Maximum braking increment
    uint16_t accelSteps;         ///< Maximum acceleration increment
    uint16_t brakeMargin;        ///< Minimum offset from neutral while actively braking

    // --- Scaling percentages (1–200, 100 = nominal) ---
    uint8_t  globalAccelPct;     ///< Global ramp scale — all gears (100 = nominal)
    uint8_t  lowRangePct;        ///< Low-range ramp time multiplier (e.g. 50 = half speed)
    uint8_t  autoRevAccelPct;    ///< Automatic-reverse acceleration boost (200 = 2× faster)

    // --- Optional output-curve correction hook ---
    EscLinearizeFn linearizeFn;  ///< nullptr = passthrough (linear ESC).

    // --- ComBus output ---
    ComBus*   comBus;            ///< Target ComBus; nullptr = skip ComBus write.
    ChanOwner owner;             ///< SYSTEM (local) or SYSTEM_EXT (external, transitional).
};


// =============================================================================
// 4. RUNTIME STATE  (one instance per device, owned by DcDevice)
// =============================================================================

/**
 * @brief Per-instance mutable state for the inertia FSM.
 *
 * @details Stored inline in `DcDevice::motionRt` so each motor instance
 *   carries its own independent ramp state.  The FSM API (`esc_inertia_update`)
 *   receives a reference to this struct rather than using module-level statics,
 *   enabling multiple simultaneous motor instances.
 *
 *   Clone entries (`DcDevice::parentID` set) must never copy this struct from
 *   their parent — configs are shared, but runtime state is always per-instance.
 *   `applyParentConfig()` copies `motion` (pointer) and never touches `motionRt`.
 */
struct EscInertiaRuntime {
    uint16_t  cbusPos       = 32767u;  ///< Current inertial position (ComBus units)
    int8_t    driveState    = 0;       ///< FSM state index (0–4, see esc_inertia.cpp)
    uint16_t  driveRampRate = 1u;      ///< Acceleration increment for this cycle
    uint16_t  brakeRampRate = 1u;      ///< Braking increment for this cycle
    uint8_t   driveRampGain = 1u;      ///< Clutch-engagement multiplier (1 or 2 or 4)
    uint32_t  rampMillis    = 0u;      ///< Timestamp of last ramp step (millis())
};


// =============================================================================
// 5. OUTPUT SNAPSHOT  (per-cycle, produced by esc_inertia_update)
// =============================================================================

/**
 * @brief Per-cycle output state produced by the inertia FSM.
 *
 * @details Consumed by the sound engine (`engineMassSimulation`, `mapThrottle`)
 *   and by the ESC output stage.  `airBrakeTrigger` is a one-shot pulse —
 *   the caller must process it within the same cycle it is set.
 */
struct EscInertiaState {
    uint16_t cbusPos;          ///< Inertial position (0–65535, pre-linearize)
    uint16_t escCbusVal;       ///< Mapped ESC command (post-linearize + scaling)
    uint16_t currentSpeed;     ///< 0–500 proportional to inertial deviation from neutral
    bool     escIsBraking;     ///< FSM is in a braking state
    bool     escInReverse;     ///< FSM is driving in reverse
    bool     escIsDriving;     ///< FSM is actively driving (forward or reverse)
    bool     brakeDetect;      ///< Stick and inertia position are on opposing sides
    bool     airBrakeTrigger;  ///< One-shot: pulsed true on the cycle inertial motion stops
};

// EOF esc_inertia_struct.h
