/*!****************************************************************************
 * @file  simulation_struct.h
 * @brief Simulation layer structures — archived SimDev + active CbChain pipeline.
 *
 * @details Archive (A0–A4, above the separator) — old SimDev architecture,
 *   preserved until sim_traction, sim_gear and sim_brake are migrated.
 *   Do not add new code to the archive.
 *
 *   Active area (sections 1+, below the separator) — CbChain pipeline.
 *   Already validated: `CbProc/CbChain` (sim.cpp ✅, section 3),
 *   `DriveState` (✅, section 2).
 *   `CbBypassCfg` → moved to `include/struct/combus/processors/base/cb_bypass_struct.h`.
 *   `CbRampCfg/State` → moved to `include/struct/combus/processors/motion/cb_ramp_struct.h`.
 *   `GearProcCfg` (gear_fsm_fn ✅, gear module in processors/modules/gear/).
 *
 *   **Adding a new CbProcFn:**
 *   1. Add `MyProcCfg` and `MyProcState` in the active area (sections 1+).
 *   2. Create `sim_<name>.h/.cpp` in `src/core/system/simulation/`.
 *   3. Nothing else changes here.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <optional>
#include <variant>

#include <core/config/machines/combus_ids.h>  // AnalogComBusID, DigitalComBusID
#include <struct/combus_struct.h>              // ComBus (needed for SimBehaviorFn in archive)
#include <struct/combus_proc_struct.h>                  // CbProc, CbChain, CbProcFn
#include <defs/defs.h>                                         // ChanOwner
#include <struct/combus/processors/modules/gear_struct.h>      // GearStepCfg, GearShiftProfile, GearFsmState, GearProcCfg (migrated)


// =============================================================================
// 1. DRIVE STATE  (DriveStateBus wire encoding — ✅ implemented)
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
// │  CbChain.  Ne pas ajouter de nouveau code ici.                       │
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
    uint16_t defaultRampTime;       ///< Fallback inertia ramp duration (ms) — used by legacy sim_traction when no
                                    ///<   per-gear ramp source is connected.
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
// A2. GEAR FSM — migrated to combus/processors/modules/gear_struct.h
// =============================================================================
//   GearSimCfg   (legacy SimDev — archived)
//   GearFsmState →  include/struct/combus/processors/modules/gear_struct.h
//   (struct imported via top-level include)


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
// A4. SimDev DESCRIPTOR  (ancienne architecture — remplacée par CbChain)
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
    ChanOwner      chainOwner = ChanOwner::MACHINE_SYSTEM; ///< Identity passed to combus_set_* when writing output channels.
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
//   CbProcFn  →  include/struct/combus_proc_struct.h
//   CbProc    →  include/struct/combus_proc_struct.h
//   CbChain   →  include/struct/combus_proc_struct.h
//   (structs imported via top-level include)


// =============================================================================
// 4. GEAR FSM PROCESSOR — migrated to combus/processors/modules/gear_struct.h
// =============================================================================
//   GearProcCfg      →  include/struct/combus/processors/modules/gear_struct.h
//   GearFsmState     →  include/struct/combus/processors/modules/gear_struct.h
//   ShiftDeltaState  →  include/struct/combus/processors/modules/gear_struct.h
//   (structs imported via top-level include)


// =============================================================================
// 6. GENERIC ARITHMETIC PROC CONFIGS  — moved to combus/processors/math/
// =============================================================================
//   CbCenterCfg  →  include/struct/combus/processors/math/cb_center_struct.h
//   CbScaleCfg   →  include/struct/combus/processors/math/cb_scale_struct.h
//   cb_abs_fn: cfg = nullptr (no struct needed)


// =============================================================================
// 7. SUB-GEAR BUTTON PROCESSOR — RETIRED (replaced by cb_btn.h INPUT chains)
// =============================================================================

// Sub-gear button handling now lives in combus_input_config.cpp (INPUT chains).
// SimSubGearBtnCfg / SimSubGearBtnState removed — replaced by CbBtnCfg / CbBtnState.


// EOF simulation_struct.h
