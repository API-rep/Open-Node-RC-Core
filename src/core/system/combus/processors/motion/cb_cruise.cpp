/******************************************************************************
 * @file  cb_cruise.cpp
 * @brief CbProc function — throttle hold (cruise-control).
 *****************************************************************************/

#include "cb_cruise.h"
#include <core/system/combus/combus_res.h>  // CbusNeutral


// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

/**
 * @brief Returns true if `val` represents a strictly higher speed than `held`
 *        in the same direction.
 *
 * @details Works in the bipolar domain [0..CbusMaxVal] around CbusNeutral.
 *   Forward (both > CbusNeutral): val > held.
 *   Reverse (both < CbusNeutral): val < held (more negative = faster).
 *   Different directions or either at neutral: returns false.
 */
static bool speedAboveHeld(uint16_t val, uint16_t held)
{
    if (val > CbusNeutral && held > CbusNeutral) return val > held;
    if (val < CbusNeutral && held < CbusNeutral) return val < held;
    return false;
}

/** @brief Sync ramp currentPos to heldValue so the ramp restarts cleanly on deactivation. */
static void syncRamp(const CbCruiseCfg* cfg, uint16_t heldValue)
{
    if (cfg->rampState != nullptr) {
        cfg->rampState->currentPos = heldValue;
    }
}


// =============================================================================
// 2. PROC FUNCTION
// =============================================================================

/** @brief Throttle hold — see cb_cruise.h for full contract. */
void cb_cruise_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const CbCruiseCfg* cfg   = static_cast<const CbCruiseCfg*>(proc->cfg);
    CbCruiseState*     state = static_cast<CbCruiseState*>(proc->state);

    // Determine activation.
    // holdSpeed: active when subgear inCh is nonzero OR state->active (set by cruise_sync from CRUISE_ACTIVE).
    // normal: active from state->active only.
    const bool isActive = cfg->holdSpeed ? ((proc->inValue != 0u) || state->active) : state->active;

    // --- Passthrough / falling edge -----------------------------------------
    if (!isActive) {
        if (state->wasActive) {
            syncRamp(cfg, state->heldValue);  // Prevent jerk: ramp resumes from held.
            state->wasActive = false;
        }
        return;
    }

    // --- Rising edge: latch current value -----------------------------------
    if (!state->wasActive) {
        state->heldValue = value;
        state->wasActive = true;
    }

    // --- Pending "update cruise speed" request (button or internal logic) ---
    if (state->updateReq) {
        state->heldValue = value;
        state->updateReq = false;
    }

    // --- Braking detection: extBrakeSteps > 0 set by brake proc this cycle --
    const bool braking = (cfg->dynRampCfg != nullptr
                          && cfg->dynRampCfg->extBrakeSteps > 0);

    // --- Mode logic ---------------------------------------------------------
    if (!cfg->holdSpeed) {
        // Normal mode: floor hold. Deactivate on braking.
        if (braking) {
            state->active    = false;
            state->wasActive = false;
            syncRamp(cfg, state->heldValue);
            return;  // Pass through this cycle.
        }
        if (speedAboveHeld(value, state->heldValue)) {
            state->heldValue = value;  // Track upward.
            // value passes through unchanged.
        } else {
            value = state->heldValue;  // Hold floor.
        }

    } else {
        // holdSpeed mode: adaptive watermark (auto-activation via inCh).
        if (braking) {
            // Active braking: follow speed downward; hold at braked-to speed.
            state->heldValue = value;
            // Pass through — ramp decelerates freely.

        } else if (speedAboveHeld(value, state->heldValue)) {
            // Speed exceeded held → track upward.
            state->heldValue = value;
            // Pass through — ramp accelerates freely.

        } else {
            // At or below held → floor hold.
            value = state->heldValue;
        }
    }
}


// =============================================================================
// 3. HELPER PROCS (passthrough — sync state from ComBus channels)
// =============================================================================

/** @brief Sync state->active from CRUISE_ACTIVE ComBus channel — see cb_cruise.h. */
void cb_cruise_sync_fn(CbProc* proc, uint16_t& /*value*/, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    CbCruiseState* state = static_cast<CbCruiseState*>(proc->state);
    state->active = (proc->inValue != 0u);
}

/** @brief Set state->updateReq when CRUISE_UPDATE_BTN pressed — see cb_cruise.h. */
void cb_cruise_upd_fn(CbProc* proc, uint16_t& /*value*/, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    CbCruiseState* state = static_cast<CbCruiseState*>(proc->state);
    if (proc->inValue != 0u) {
        state->updateReq = true;
    }
}

// EOF cb_cruise.cpp

// EOF cb_cruise.cpp
