/*!****************************************************************************
 * @file  simulation_struct.h
 * @brief Simulation layer structures — archived SimDev + active SimChannel pipeline.
 *
 * @details Archive (A0–A4, above the separator) — old SimDev architecture,
 *   preserved until sim_traction, sim_gear and sim_brake are migrated.
 *   Do not add new code to the archive.
 *
 *   Active area (sections 1+, below the separator) — SimChannel pipeline.
 *   Already validated: `SimProc/SimChannel` (sim.cpp ✅, section 3),
 *   `SimRampCfg/State` (sim_ramp_fn ✅, section 1), `DriveState` (✅, section 2),
 *   `SimBypassCfg` (sim_bypass_fn ✅, section 4).
 *   `GearProcCfg` (sim_gear_fn ✅, section 5).
 *
 *   **Adding a new SimProcFn:**
 *   1. Add `MyProcCfg` and `MyProcState` in the active area (sections 1+).
 *   2. Create `sim_<name>.h/.cpp` in `src/core/system/simulation/`.
 *   3. Nothing else changes here.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <optional>

#include <core/config/machines/combus_ids.h>  // AnalogComBusID, DigitalComBusID
#include <struct/combus_struct.h>              // ComBus (needed for SimBehaviorFn in archive)
#include <defs/defs.h>                         // ChanOwner


// =============================================================================
// 0. VIRTUAL GEAR PROFILE  (shared between sim_gear and motion presets)
// =============================================================================

/**
 * @brief Threshold and inertia config for one virtual gear.
 *
 * @details `upShift` is the RPM at which the FSM transitions from this gear to
 *   the next.  `downShift` / `downShiftBraking` are the thresholds below which
 *   the FSM drops back one gear (coasting / braking respectively).
 *   Hysteresis requirement: `upShift > downShift` — prevents gear hunting.
 *
 *   `shiftDelta` is the RPM drop applied when upshifting INTO this gear:
 *   the virtual RPM used by the FSM drops by this amount at the moment of
 *   the shift, simulating the engine RPM fall as the new ratio takes effect.
 *   gear[0].shiftDelta is ignored (no upshift into gear 1).
 *   `maxRpm` of the profile equals the last gear's `upShift` by convention.
 */
struct GearStepCfg {
    int16_t  upShift;           ///< RPM threshold to upshift from this gear (= maxRpm for last gear).
    int16_t  downShift;         ///< RPM threshold to downshift to this gear (coasting).
    int16_t  downShiftBraking;  ///< RPM threshold to downshift to this gear (braking — higher → earlier).
    uint16_t rampTime;          ///< Inertia ramp duration (ms) — written to TRACTION_RAMP_BUS.
    int16_t  shiftDelta;        ///< RPM drop when upshifting INTO this gear (ignored for gear 1).
};

/**
 * @brief Config for one sub-gear step.
 *
 * @details Index 0 = slowest crawl, index N-1 = fastest crawl.
 *   All sub-gear ramp times are slower than normal gear-1 `rampTime`.
 *   `maxPct` caps the RPM input to the gear FSM while this sub-gear is active:
 *   expressed as a percentage (0–100) of `gear[0].upShift` (the gear-1 RPM ceiling).
 *   Full throttle in sub-gear n maps to `gear[0].upShift × maxPct / 100`.
 */
struct SubGearStepCfg {
    uint16_t rampTime;  ///< Inertia ramp duration (ms) for this sub-gear.
    uint8_t  maxPct;   ///< RPM ceiling as % of gear[0].upShift (0–100).
};

/**
 * @brief Speed-threshold profile for a virtual N-speed gearbox FSM.
 *
 * @details All speed values are in RPM units.
 *   `gear[n]` covers the (n+1)-th gear (0-based index).
 *   `gear[n].upShift` is the RPM threshold to shift from gear n+1 to n+2.
 *   `gear[gearCount-1].upShift` is the maximum RPM (scaling reference).
 *   Hysteresis requirement: `gear[n].upShift > gear[n].downShift`.
 *
 *   Presets are declared `constexpr` in `simulation_presets.h` and exposed
 *   via vehicle aliases (`*_motion.h`).
 *   The gear FSM lives inside `sim_gear.cpp` — no separate `gear_fsm.h`.
 */
struct GearShiftProfile {
    uint8_t               gearCount;    ///< Number of active gears (= std::size(gear[])).
    const GearStepCfg*    gear;         ///< Per-gear config — array[gearCount].
    uint16_t              shiftGuardMs; ///< Minimum interval between consecutive shifts (ms).

    uint8_t               subGearCount; ///< Number of sub-gears in gear 1 (0 = sub-gear disabled).
    const SubGearStepCfg* subGear;      ///< Per-sub-gear config — array[subGearCount]; nullptr when subGearCount == 0.
};


// =============================================================================
// 2. DRIVE STATE  (DriveStateBus wire encoding — ✅ implemented)
// =============================================================================

/**
 * @brief 5-state drive FSM encoded as signed int8_t.
 *
 * @details Sign gives direction, magnitude distinguishes driving from braking:
 *   - `state > 0` → forward,  `state < 0` → reverse
 *   - `abs(state) == 1` → braking,  `abs(state) == 2` → driving
 *   - `state == 0` → standing still
 *
 *   Traction mode uses all 5 states.  Simple ramp only uses 0 / +2 / −1
 *   (braking states require an inertia model).
 */
namespace DriveState {
    constexpr int8_t kBrakeRev  = -2;  ///< Decelerating in reverse (toward neutral).
    constexpr int8_t kDriveRev  = -1;  ///< Accelerating or cruising in reverse.
    constexpr int8_t kStanding  =  0;  ///< No motion.
    constexpr int8_t kBrakeFwd  = +1;  ///< Decelerating forward (toward neutral).
    constexpr int8_t kDriveFwd  = +2;  ///< Accelerating or cruising forward.
}

/**
 * @brief ComBus encoding helpers for the DRIVE_STATE_BUS analog wire channel.
 *
 * @details Packs the 5-state DriveState FSM into a 3-bit uint16_t value:
 *   - bit0 (ACTIVE) : 1 when moving (not standing still)
 *   - bit1 (FWD)    : 1 when direction is forward
 *   - bit2 (BRAKE)  : 1 when in a braking state
 *
 *   kStanding = 0  (no bits set)
 *   kDriveRev = 1  (ACTIVE only)
 *   kDriveFwd = 3  (ACTIVE + FWD)
 *   kBrakeRev = 5  (ACTIVE + BRAKE)
 *   kBrakeFwd = 7  (ACTIVE + FWD + BRAKE)
 *
 * @note Winter 2026 / ComBus v2: DRIVE_STATE_BUS will be superseded by a packed
 *   TRACTION_BUS channel: [bit15:BRAKE | bit14:FWD | 13-bit magnitude (0..8191)].
 *   At that point, remove this namespace and rebuild encode()/decode() as part of
 *   the TRACTION_BUS helpers.  ESC_SPEED_BUS and DRIVE_STATE_BUS are then merged.
 */
namespace DriveStateBus {
    constexpr uint16_t kMaskActive = 0x01u; ///< Bit 0 — motion active (not standing).
    constexpr uint16_t kMaskFwd    = 0x02u; ///< Bit 1 — direction forward.
    constexpr uint16_t kMaskBrake  = 0x04u; ///< Bit 2 — braking state.

    /// @brief Encode a DriveState int8_t to the DRIVE_STATE_BUS 3-bit wire value.
    constexpr uint16_t encode(int8_t ds)
    {
        if (ds == DriveState::kStanding) return 0u;
        uint16_t v = kMaskActive;
        if (ds == DriveState::kDriveFwd || ds == DriveState::kBrakeFwd) v |= kMaskFwd;
        if (ds == DriveState::kBrakeFwd || ds == DriveState::kBrakeRev) v |= kMaskBrake;
        return v;
    }

    /// @brief Decode a DRIVE_STATE_BUS wire value back to the DriveState constant.
    constexpr int8_t decode(uint16_t v)
    {
        if (!(v & kMaskActive)) return DriveState::kStanding;
        const bool fwd   = (v & kMaskFwd)   != 0u;
        const bool brake = (v & kMaskBrake) != 0u;
        if ( fwd && !brake) return DriveState::kDriveFwd;
        if (!fwd && !brake) return DriveState::kDriveRev;
        if ( fwd &&  brake) return DriveState::kBrakeFwd;
        return DriveState::kBrakeRev;
    }
}


// =============================================================================
// ┌─── ARCHIVE TEMPORAIRE ──────────────────────────────────────────────────┐
// │  Ancienne architecture SimDev — conservée tant que les callers          │
// │  existants ne sont pas migrés vers la nouvelle implémentation           │
// │  SimChannel.  Ne pas ajouter de nouveau code ici.                       │
// │  À supprimer une fois sim_traction, sim_gear et sim_brake migrés.       │
// └─────────────────────────────────────────────────────────────────────────┘
// =============================================================================


// =============================================================================
// A1. TRACTION INERTIA  (sim_traction — ⚠ non migré, winter 2026)
// =============================================================================

/**
 * @brief Static configuration for a traction inertia SimDev.
 *
 * @details Assigned to `SimDev::cfg` (as `const void*`); cast back inside
 *   `sim_traction_update()` to `const SimTractionCfg*`.
 */
struct SimTractionCfg {
    uint16_t defaultRampTime;       ///< Fallback inertia ramp duration (ms) — used when TRACTION_RAMP_BUS is zero
                                    ///<   (no sim_gear upstream, or first cycle before sim_gear has written).
    uint16_t brakeSteps;            ///< Deceleration step size (ComBus units per ramp tick).
    uint16_t accelSteps;            ///< Acceleration step size (ComBus units per ramp tick).
    uint16_t neutralBand;           ///< ComBus units around CbusNeutral treated as STAND.
    uint16_t brakeMargin;           ///< Inertia hold-off during braking — prevents rolling back to neutral (ComBus units).
    bool     shiftingAutoThrottle;  ///< Enable gear-sync throttle correction on shift events (manual gearbox only).
    uint16_t clutchEngagingSpeed;   ///< Speed below which driveRampGain doubles to prevent clutch slip (0–500 domain).
    uint8_t  lowRangePct;           ///< Ramp-time scale in low-range mode (< 100 % = slower, > 100 % = faster).
    uint8_t  autoReverseAccelPct;   ///< Ramp-time scale in automatic-reverse (> 100 % = faster).
};

/**
 * @brief Mutable runtime state for a traction inertia SimDev.
 *
 * @details Written exclusively by `sim_traction_update()` — internal FSM only.
 *   External modules must not read this struct directly: all externally
 *   visible state is written to the ComBus digital output channels declared
 *   in SimDev (outRevCh, outBrakeCh, outDriveCh).
 *   Zero-initialised by default construction.
 */
struct SimTractionState {
    uint16_t inertiaPos;      ///< Current inertial position in ComBus domain [0..CbusMaxVal].
    int8_t   driveState;      ///< Drive FSM state — see DriveState namespace (simulation_struct.h).
    bool     brakeDetect;     ///< Latched braking transition flag (cleared by FSM).
    uint16_t currentRampTime; ///< Active ramp duration this cycle (ms), gear-dependent.
    uint32_t lastUpdateMs;    ///< millis() timestamp of last update call.
    int8_t   prevGear;        ///< Gear seen last cycle — shift-pulse detection (upshift / downshift).
};


// =============================================================================
// A2. GEAR FSM  (sim_gear — ⚠ non migré, winter 2026)
// =============================================================================

/**
 * @brief Static configuration for a gear-FSM SimDev.
 *
 * @details Assigned to `SimDev::cfg` (as `const void*`); cast back inside
 *   `sim_gear_update()` to `const GearSimCfg*`.
 */
struct GearSimCfg {
    const GearShiftProfile* profile; ///< Shift threshold profile — pointer via vehicle alias.
};

/**
 * @brief Mutable runtime state for a gear-FSM SimDev.
 *
 * @details Assigned to `SimDev::state` (as `void*`); cast back inside
 *   `sim_gear_update()` and `gear_fsm_update()` to `GearFsmState*`.
 */
struct GearFsmState {
    int8_t   gear;         ///< Current virtual gear (1–N).
    int16_t  prevRpm;      ///< RPM seen last cycle — trend detection (rising = accel, falling = decel).
    uint32_t lastShiftMs;  ///< Timestamp of last shift — anti-hunting guard.

    int8_t   subGear;        ///< Active sub-gear index (1..subGearCount). 0 = sub-gear mode inactive.
    bool     prevSubGearSet; ///< Previous state of SUBGEAR_SET — rising-edge detection.
    bool     prevSubGearUp;  ///< Previous state of SUBGEAR_UP  — rising-edge detection.
    bool     prevSubGearDn;  ///< Previous state of SUBGEAR_DOWN — rising-edge detection.
};


// =============================================================================
// A3. BRAKE AXIS  (sim_brake — ⚠ non migré, winter 2026)
// =============================================================================

/**
 * @brief Static configuration for a brake-axis SimDev.
 *
 * @details Assigned to `SimDev::cfg` (as `const void*`); cast back inside
 *   `sim_brake_update()` to `const SimBrakeCfg*`.
 *
 *   Place this SimDev at a lower array index than `sim_traction` — when the
 *   brake is active, `sim_brake_update()` writes `outCh` (ESC_SPEED_BUS),
 *   setting `isDrived = true` via the clone guard and preventing a downstream
 *   `sim_traction` from overwriting the braked position.
 */
struct SimBrakeCfg {
    uint16_t brakeStep;   ///< Position change applied to `outCh` per ramp tick (ComBus units).
    uint16_t minRampMs;   ///< Ramp tick interval at maximum brake input (fastest deceleration, ms).
    uint16_t maxRampMs;   ///< Ramp tick interval at minimum brake input (slowest deceleration, ms).
};

/**
 * @brief Mutable runtime state for a brake-axis SimDev.
 *
 * @details Written exclusively by `sim_brake_update()`.
 *   Zero-initialised by default construction.
 */
struct SimBrakeState {
    uint32_t lastUpdateMs;  ///< millis() timestamp of last ramp step.
};


// =============================================================================
// A4. SimDev DESCRIPTOR  (ancienne architecture — remplacée par SimChannel)
// =============================================================================

struct SimDevCtx;  ///< Legacy context — used by SimBehaviorFn only.
struct SimDev;

/// Legacy behaviour function pointer — one per SimDev instance.
using SimBehaviorFn = void (*)(SimDev* dev, ComBus& bus, const SimDevCtx& ctx);

/**
 * @brief ComBus simulation device descriptor.
 *
 * @details Pure ComBus processor: reads one or more input channels, applies
 *   a time-domain behaviour (ramp, FSM, envelope…), and writes the result to
 *   one or more output channels.  No hardware port, no pin reference.
 *
 *   `cfg` and `state` are `void*` — each behaviour function casts them to its
 *   own concrete cfg / state type.  This keeps SimDev generic without unions
 *   or virtual dispatch.
 *
 * @note Array order: a SimDev that reads the outCh of another must appear
 *   at a higher index in the `simDev[]` array (sim_gear before sim_traction).
 *
 * @note Clone guard: if two SimDev entries share the same outCh,
 *   sim_dev_update() skips the second — outCh was already written this cycle.
 */
struct SimDev {
    const int8_t   ID;       ///< Device ID (enum from machine header).
    const char*    infoName; ///< Short description.
    const void*    cfg;      ///< Behaviour-specific config (flash) — cast inside the behaviour fn.
    void*          state;    ///< Behaviour-specific runtime state (RAM) — cast inside the behaviour fn.

    // --- ComBus I/O channels -------------------------------------------------
    AnalogComBusID inCh;     ///< Primary analog input channel.
    AnalogComBusID outCh;    ///< Primary analog output channel.

    // --- Optional secondary channels -----------------------------------------
    std::optional<AnalogComBusID>  gearCh;          ///< Gear channel (written by sim_gear) — read by sim_traction for ramp selection.
    std::optional<DigitalComBusID> outRevCh;         ///< Reverse flag → ComBus digital (sound, reverse lights…).
    std::optional<DigitalComBusID> outBrakeCh;       ///< Braking flag → ComBus digital (brake lights…).
    std::optional<DigitalComBusID> outDriveCh;       ///< Driving flag → ComBus digital (running lights…).
    std::optional<AnalogComBusID>  outDriveStateCh;  ///< Encoded DriveState → analog ComBus (e.g. DRIVE_STATE_BUS).

    // --- Behaviour -----------------------------------------------------------
    SimBehaviorFn  behaviorFn;                           ///< Processing function. nullptr = no-op.
    ChanOwner      chanOwner = ChanOwner::MACHINE_SYSTEM; ///< Identity passed to combus_set_* when writing output channels.
};


// =============================================================================
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  FIN DU CODE ARCHIVÉ — SECTIONS A1 À A4 CI-DESSUS                      │
// │                                                                         │
// │  NOUVELLE IMPLÉMENTATION — SECTIONS 1+ CI-DESSOUS                      │
// └─────────────────────────────────────────────────────────────────────────┘
// =============================================================================


// =============================================================================
// 3. SIMCHANNEL PIPELINE  (sim.cpp — ✅ implemented)
// =============================================================================

// Forward declaration — SimProcFn references SimProc by pointer.
struct SimProc;

/// Processor function — one per SimProc instance, called once per channel per cycle.
/// @param proc    Processor descriptor (name, inCh, cfg, state).
/// @param value   Channel value (in/out) — seeded from bus[SimChannel::inCh].value before proc 0.
/// @param claimed Set to true to short-circuit remaining processors this cycle.
/// @param bus     Shared ComBus — read proc->inCh; do NOT write SimChannel::outCh (channel owns the write).
using SimProcFn = void (*)(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);


/**
 * @brief One processing unit within a SimChannel pipeline.
 *
 * @note **Mutable config pattern:** `cfg` is flash-const (`const void*`).  If a
 *   processor needs runtime-varying configuration (beyond internal state), embed a
 *   `const MyCfg* dynCfg = nullptr` field in its `MyProcState` struct.  Inside the
 *   fn, resolve the effective config as:
 *   @code
 *   const MyCfg* effective = state->dynCfg ? state->dynCfg
 *                                           : static_cast<const MyCfg*>(proc->cfg);
 *   @endcode
 *   A preceding processor in the same pipeline (e.g. sim_gear_fn) can write
 *   `state->dynCfg` before this proc runs, enabling per-cycle config switching
 *   without touching flash.
 */
struct SimProc {
    const char*                        name;      ///< Debug / dashboard label.
    std::optional<AnalogComBusID>      optInCh;   ///< Auxiliary analog input read by the proc (e.g. TRACTION_RAMP_BUS). std::nullopt when unused.
    std::optional<DigitalComBusID>     optOutDCh; ///< Optional digital side-effect output written by the proc (e.g. sign from sim_abs_fn). std::nullopt when unused.
    SimProcFn                          fn;        ///< C function pointer — assigned in sim_config.cpp (e.g. &sim_ramp_fn). nullptr = passthrough.
    const void*                        cfg;       ///< Flash — static config struct (e.g. SimRampCfg). Cast inside fn.
    void*                              state;     ///< RAM   — mutable runtime state (e.g. SimRampState). Cast inside fn.
};


/**
 * @brief One named processing channel — ordered processor list, single ComBus output.
 *
 * @details Defines *what* to compute and *where* to write the result.
 *   The ComBus is not stored here — it is provided at call time by
 *   `sim_update()` / `sim_channel_update()`.
 *
 *   Update sequence (sim_channel_update):
 *   1. Seed `value` from `bus.analogBus[inCh].value` — live input captured at cycle start.
 *   2. Iterate `simProc[]` in order; stop early when a processor sets `claimed = true`.
 *   3. Write `value` to `outCh` via `combus_set_analog()` — always, regardless of claimed.
 */
struct SimChannel {
    const char*     name;         ///< Human-readable channel name (debug / dashboard).
    AnalogComBusID  inCh;         ///< Input channel — seeded into `value` at cycle start.
    AnalogComBusID  outCh;        ///< Output channel — written after all processors complete.
    SimProc*        simProc;      ///< Processor array (nullptr when simProcCount == 0).
    uint8_t         simProcCount; ///< Number of processors in simProc[].
    ChanOwner       chanOwner;    ///< Identity token passed to combus_set_analog().
};


// =============================================================================
// 1. RAMP PROCESSOR  (sim_ramp_fn — ✅ implemented)
// =============================================================================

/**
 * @brief Static configuration for a single-axis inertia ramp SimProc.
 *
 * @details Assigned to `SimProc::cfg` (as `const void*`); cast back to
 *   `const SimRampCfg*` inside `sim_ramp_fn()`.
 *
 *   Multi-instance: one SimProc entry per axis that needs inertia
 *   (e.g. DUMP_BUS → DUMP_RAMPED_BUS, STEERING_BUS → STEERING_RAMPED_BUS).
 *   The downstream `DcDevice` reads the ramped output channel.
 */
struct SimRampCfg {
    uint16_t rampTimeMs;                    ///< Default period between ramp steps (ms).
    uint16_t accelSteps;                    ///< ComBus units per step when moving away from neutral (both directions if accelDownSteps == 0).
    uint16_t accelDownSteps = 0u;           ///< ComBus units per step when moving in the NEGATIVE direction away from neutral.
                                            ///<   0 = symmetric (falls back to accelSteps).
    uint16_t brakeSteps;                    ///< ComBus units per step when moving toward neutral.
    uint16_t neutralBand;                   ///< ComBus units around CbusNeutral treated as zero (0 = no dead-band).
    bool     rampTimeFromBus = false; ///< When true, override rampTimeMs with TRACTION_RAMP_BUS value each cycle.
                                            ///<   TRACTION_RAMP_BUS is written by sim_gear_fn per active gear.
                                            ///<   Falls back to rampTimeMs when the channel value is 0.
};

/**
 * @brief Mutable runtime state for a ramp SimProc.
 *
 * @details Written exclusively by `sim_ramp_fn()`.
 *   Zero-initialised by default construction — `currentPos == 0` triggers
 *   a self-init to CbusNeutral on the first call.
 */
struct SimRampState {
    uint16_t currentPos;    ///< Current inertial position in ComBus domain [0..CbusMaxVal].
    uint32_t lastUpdateMs;  ///< millis() timestamp of last ramp step.
                            ///<   Also used as first-call sentinel: 0 = never initialised.
                            ///<   Do NOT use currentPos == 0 — 0 is a valid position (full negative).
};


// =============================================================================
// 4. BYPASS PROCESSOR  (sim_bypass_fn — ✅ implemented)
// =============================================================================

/**
 * @brief Static configuration for a conditional bypass gate SimProc.
 *
 * @details When `condCh` digital channel is HIGH, `claimed` is set to `true`
 *   and all downstream processors are skipped for this cycle.  The raw `inCh`
 *   value passes through to `outCh` unchanged (sim_channel_update always
 *   writes after all procs, regardless of `claimed`).
 *
 *   Assign to `SimProc::cfg` (as `const void*`); cast back inside
 *   `sim_bypass_fn()`.  No runtime state required — `SimProc::state` must be
 *   `nullptr`.
 *
 *   Typical placement: `simProc[0]` — evaluated before all other processors.
 */
struct SimBypassCfg {
    DigitalComBusID condCh;  ///< Digital channel — HIGH → early exit (raw passthrough mode).
};


// =============================================================================
// 5. GEAR FSM PROCESSOR  (sim_gear_fn — ✅ implemented)
// =============================================================================

/**
 * @brief Static configuration for a gear-FSM SimProc.
 *
 * @details Shared by `sim_gear_fn` and `sim_apply_ratio_fn`.
 *   Assigned to `SimProc::cfg` (as `const void*`); cast back to
 *   `const GearProcCfg*` inside each fn.
 *
 *   `GearFsmState` (mutable runtime for `sim_gear_fn`) is declared in
 *   archive section A2 and is available throughout the active area.
 */
struct GearProcCfg {
    const GearShiftProfile* profile; ///< Shift threshold profile — pointer via vehicle alias.
};

/**
 * @brief Mutable runtime state for `sim_apply_ratio_fn`.
 *
 * @details Assigned to `SimProc::state` (as `void*`); cast back inside
 *   `sim_apply_ratio_fn()`.  Zero-init is valid (·prevGear = 0· means
 *   no upshift on first cycle).
 */
struct ShiftDeltaState {
    int8_t prevGear; ///< Gear seen last cycle — upshift edge detection.
};


// =============================================================================
// 6. GENERIC ARITHMETIC PROC CONFIGS  (sim_math.h — ✅ implemented)
// =============================================================================

/**
 * @brief Configuration for `sim_scale_fn` — linear domain rescale.
 *
 * @details `value = value × outMax / inMax`.
 *
 *   `sim_center_fn` and `sim_abs_fn` are **cfg-free** (`SimProc::cfg = nullptr`):
 *   - `sim_center_fn`: pure signed deviation from CbusNeutral — no config needed.
 *   - `sim_abs_fn`: sign side effect declared via `SimProc::optOutDCh`
 *     (std::nullopt = skip, otherwise writes HIGH/LOW to that digital channel).
 *
   *   Typical three-proc chain for THROTTLE_BUS → RPM_BUS:
 *   @code
 *     sim_center_fn  { optInCh = nullopt, optOutDCh = nullopt, cfg = nullptr }
 *     sim_abs_fn     { optOutDCh = nullopt,                    cfg = nullptr }
 *     sim_scale_fn   { cfg = &{ inMax = CbusNeutral, outMax = gear[n-1].upShift } }
 *   @endcode
 *
 *   For a pipeline that also captures direction as a digital side effect:
 *   @code
 *     sim_abs_fn     { optOutDCh = ESC_REVERSE_BUS }  // HIGH = FWD, LOW = REV
 *   @endcode
 */
struct SimScaleCfg {
    uint16_t inMax;   ///< Input range ceiling (e.g. CbusNeutral after sim_abs_fn).
    uint16_t outMax;  ///< Output range ceiling (e.g. gear[n-1].upShift — init from profile).
};


// EOF simulation_struct.h
