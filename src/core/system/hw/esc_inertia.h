/******************************************************************************
 * @file esc_inertia.h
 * @brief Stateless vehicle-inertia FSM for ESC output — core, environment-agnostic.
 *
 * @details Implements two motion-control algorithms selected by
 *   `EscInertiaConfig::mode` (see esc_inertia_struct.h):
 *
 *   - `RAMP_SIMPLE`  — symmetric slew-rate limiting for hydraulics, steering.
 *   - `TRACTION_FSM` — 5-state drive/brake machine (standing, forward,
 *     braking-forward, reverse, braking-backward) that simulates vehicle mass
 *     by ramping the ESC command over time.
 *
 *   **Stateless design:** all mutable state is held in `EscInertiaRuntime`
 *   (embedded in `DcDevice::motionRt`).  The caller passes a reference to the
 *   runtime struct so that multiple motor instances can run independently
 *   without any module-level statics.
 *
 *   **ComBus write:** `esc_inertia_update()` writes `ESC_SPEED_BUS` via the
 *   `ComBus*` pointer stored in `EscInertiaConfig::bus`.  The ownership tag
 *   `EscInertiaConfig::owner` determines whether this node is the authoritative
 *   writer:
 *   - `ChanOwner::SYSTEM`     — this node owns the channel and writes it.
 *   - `ChanOwner::SYSTEM_EXT` — transitional; DiYGuy FSM in sound_module/main.cpp
 *     still writes.  Remove once migration complete.
 *
 *   **Usage (stateless):**
 *   @code
 *     // Config lives in machine config, e.g. kMotion_Traction_Truck (esc_presets.h).
 *     // Runtime struct is inline in DcDevice (per motor instance).
 *     EscInertiaInputs inp { .engineRunning = true, .selectedGear = 2, ... };
 *     EscInertiaState  state{};
 *     esc_inertia_update(cbusVal, inp, dev.motion, dev.motionRt, &state);
 *     if (state.airBrakeTrigger) trigger_air_brake_sound();
 *   @endcode
 *
 *   **Activation:** `-D ESC_INERTIA_ENABLED` in build flags.
 *   All public functions are no-ops when the flag is absent.
 *****************************************************************************/
#pragma once

#include <cstdint>

#include <struct/esc_inertia_struct.h>   // EscLinearizeFn, MotionMode,
                                         // EscInertiaConfig, EscInertiaRuntime,
                                         // EscInertiaState


// =============================================================================
// 1. INPUTS  (filled by caller each loop cycle)
// =============================================================================

/**
 * @brief Per-cycle dynamic inputs for the inertia FSM.
 *
 * @details The caller (machine loop) fills this struct from its own state
 *   variables and passes it to `esc_inertia_update()`.  Keeping inputs
 *   explicitly named avoids hidden global dependencies and makes any
 *   future migration traceable.
 *
 *   `overrideRampTimeMs` is provided for virtual-gear-ratio configurations
 *   (VIRTUAL_3_SPEED, VIRTUAL_16_SPEED_SEQUENTIAL) where the caller pre-
 *   computes a gear-ratio-scaled ramp time.  Set to 0 for normal 3-gear use.
 *
 *   Fields below the gear block are ignored when `EscInertiaConfig::mode`
 *   is `MotionMode::RAMP_SIMPLE`.
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
    bool     gearUpShiftingPulse;     ///< One-shot: shift-up just detected
    bool     gearDownShiftingPulse;   ///< One-shot: shift-down just detected
    bool     gearUpShiftingInProgress;
    bool     gearDownShiftingInProgress;
    uint16_t currentThrottle;         ///< 0–500 from ENGINE_RPM_BUS (mapThrottle result)
    uint16_t speedLimit;              ///< 0–500 top speed (from virtual gear ratio)
    uint16_t overrideRampTimeMs;      ///< 0 = auto from selectedGear; non-zero = use directly
};


// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Advance the inertia FSM by one cycle.
 *
 * @details Reads `cbusVal` (current throttle position in ComBus units 0–65535),
 *   dispatches to the algorithm selected by `cfg->mode`, updates `rt`, writes
 *   `ESC_SPEED_BUS` into `cfg->bus`, and fills all `EscInertiaState` fields.
 *
 *   `RAMP_SIMPLE` path: clamps `rt.cbusPos` toward `cbusVal` by at most
 *   `cfg->accelSteps` / `cfg->brakeSteps` per ramp-timer cycle.  Gear, engine,
 *   and clutch fields of `inputs` are ignored.
 *
 *   `TRACTION_FSM` path: runs the full 5-state machine (standing=0, forward=1,
 *   braking-forward=2, reverse=3, braking-backward=4) gated by the ramp timer.
 *
 *   Call frequency is gated internally by the ramp timer (`rt.rampMillis`);
 *   it is safe to call every loop cycle.
 *
 * @param cbusVal  Current throttle position (0–65535, from ENGINE_RPM_BUS).
 * @param inputs   Dynamic per-cycle inputs filled by the caller.
 * @param cfg      Inertia config pointer.  nullptr = passthrough (cbusVal written directly).
 * @param rt       Per-instance mutable runtime state (DcDevice::motionRt).
 * @param[out] out Output snapshot; may be nullptr if caller does not need it.
 *
 * @note No-op when ESC_INERTIA_ENABLED is absent.
 */
void esc_inertia_update(uint16_t                cbusVal,
                        const EscInertiaInputs& inputs,
                        const EscInertiaConfig* cfg,
                        EscInertiaRuntime&      rt,
                        EscInertiaState*        out);

// EOF esc_inertia.h
