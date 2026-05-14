ok,      /******************************************************************************
 * @file  gear_fsm.h
 * @brief Speed-based virtual N-gear FSM.
 *
 * @details Computes the virtual gear (1–N) from a caller-supplied RPM value
 *          and the shift thresholds in a `GearShiftView`.
 *
 *          The caller is responsible for converting its speed signal to RPM:
 *            - Machine node:  `rpm = |currentPos − CbusNeutral| * profile.maxRpm / CbusNeutral`
 *            - Sound node:    `rpm = gEngineSimState.currentRpm`
 *
 *          The FSM lives in `src/core/system/simulation/` and is accessible
 *          to all build environments via the shared core include path.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/motion_struct.h>   // GearShiftProfile


// =============================================================================
// 1. RUNTIME STATE
// =============================================================================

/** @brief Persistent state for one gear FSM instance. */
struct GearFsmState {
    int8_t   gear;         ///< Current virtual gear (1–N).
    uint32_t lastShiftMs;  ///< Timestamp of last shift — anti-hunting guard.
};


// =============================================================================
// 2. LIFECYCLE
// =============================================================================

/** @brief Reset FSM to gear 1, clear shift guard. */
void gear_fsm_init(GearFsmState* s);


// =============================================================================
// 3. UPDATE
// =============================================================================

/**
 * @brief Update speed-based gear FSM.
 *
 * @details  Evaluates upshift / downshift thresholds from @p profile against
 *           @p rpm.  Guard timer prevents hunting.
 *
 * @param s             Persistent FSM state — updated in place.
 * @param profile       Shift thresholds — pointer via vehicle alias, deref to const ref.
 * @param rpm           Current speed in RPM — same scale as profile thresholds.
 * @param throttlePct   Forward throttle percentage 0–100 — upshift guard.
 * @param isBraking     True when the traction pipeline is in a braking state.
 * @param isReverse     True when driving in reverse — forces gear 1.
 * @return              New gear (1–N).  Caller stores as needed.
 */
int8_t gear_fsm_update(GearFsmState*            s,
                       const GearShiftProfile&  profile,
                       int16_t                  rpm,
                       uint8_t              throttlePct,
                       bool                 isBraking,
                       bool                 isReverse);


// EOF gear_fsm.h
