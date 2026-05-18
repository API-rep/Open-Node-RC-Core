/*!****************************************************************************
 * @file  simulation_struct.h
 * @brief Simulation layer structures — archived SimDev + active SimChannel pipeline.
 *
 * @details Archive (A0–A4, above the separator) — old SimDev architecture,
 *   preserved until sim_traction, sim_gear and sim_brake are migrated.
 *   Do not add new code to the archive.
 *
 *   Active area (sections 1–3, below the separator) — SimChannel pipeline.
 *   Already validated: `SimRampCfg/State` (sim_ramp_fn ✅), `DriveState` (✅),
 *   `SimProc/SimChannel` (sim.cpp ✅).
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
 * @brief Speed-threshold profile for a virtual N-speed gearbox FSM.
 *
 * @details All speed values are in RPM units (0–maxRpm).
 *   Only the first `gears` entries in each threshold array are meaningful.
 *   Index `n` covers the (n+1)↔(n+2) gear transition:
 *     index 0 = 1st↔2nd,  index 1 = 2nd↔3rd, …
 *   Hysteresis requirement: `upShift[n] > downShift[n]` — prevents gear hunting.
 *
 *   Define the threshold arrays as `static constexpr int16_t` in the preset
 *   file (`motion_presets.h`) and assign their addresses here.  Presets are
 *   declared `constexpr` and exposed via vehicle aliases (`*_motion.h`).
 *   The gear FSM lives inside `sim_gear.cpp` — no separate `gear_fsm.h`.
 */
struct GearShiftProfile {
    uint8_t         gears;             ///< Number of active gears.
    const int16_t*  upShift;           ///< Upshift RPM thresholds — pointer to array[gears].
    const int16_t*  downShift;         ///< Downshift RPM thresholds — coasting.
    const int16_t*  downShiftBraking;  ///< Downshift RPM thresholds — braking (higher → earlier).
    const uint16_t* rampTime;          ///< Per-gear inertia ramp duration (ms) — pointer to array[gears].
                                       ///<   Written to TRACTION_RAMP_BUS by sim_gear; read by sim_traction.
                                       ///<   nullptr = no ramp data (traction will use its defaultRampTime).
    int16_t         maxRpm;            ///< Maximum simulated RPM at full output (scaling reference).
    uint16_t        shiftGuardMs;      ///< Minimum interval between consecutive shifts (ms).
    uint8_t         throttleGuardPct;  ///< Minimum forward throttle % (0–100) required for upshift.

    uint8_t         subGearCount;      ///< Number of sub-gears in gear 1 (0 = sub-gear disabled).
    const uint16_t* subGearRampTime;   ///< Per-sub-gear ramp durations (ms) — pointer to array[subGearCount].
                                       ///<   Index 0 = slowest (highest sub-gear number = fastest crawl).
                                       ///<   nullptr when subGearCount == 0.
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
// 3. SIMCHANNEL PIPELINE  (sim.cpp — ✅ implemented)
// =============================================================================

// Forward declaration — SimProcFn references SimProc by pointer.
struct SimProc;

/// Processor function — one per SimProc instance, called once per channel per cycle.
/// @param proc    Processor descriptor (name, inCh, cfg, state).
/// @param value   Channel value (in/out) — seeded from bus[SimChannel::inCh].value before proc 0.
/// @param claimed Set to true to short-circuit remaining processors this cycle.
/// @param bus     Shared ComBus — read proc->inCh; do NOT write SimChannel::outCh (channel owns the write).
using SimProcFn = void (*)(SimProc* proc, uint16_t& value, bool& claimed, ComBus& bus);


/**
 * @brief One processing unit within a SimChannel pipeline.
 */
struct SimProc {
    const char*     name;    ///< Debug / dashboard label.
    AnalogComBusID  inCh;    ///< Auxiliary input channel (e.g. TRACTION_RAMP_BUS). Ignored when unused.
    SimProcFn       fn;      ///< C function pointer — assigned in sim_config.cpp (e.g. &sim_ramp_fn). nullptr = passthrough.
    const void*     cfg;     ///< Flash — static config struct (e.g. SimRampCfg). Cast inside fn.
    void*           state;   ///< RAM   — mutable runtime state (e.g. SimRampState). Cast inside fn.
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
    uint16_t rampTimeMs;   ///< Period between ramp steps (ms).
    uint16_t accelSteps;   ///< ComBus units per step when moving away from neutral.
    uint16_t brakeSteps;   ///< ComBus units per step when moving toward neutral.
    uint16_t neutralBand;  ///< ComBus units around CbusNeutral treated as zero (0 = no dead-band).
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


// EOF simulation_struct.h
