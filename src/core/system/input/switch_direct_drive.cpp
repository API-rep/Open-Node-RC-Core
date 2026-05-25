/******************************************************************************
 * @file  switch_direct_drive.cpp
 * @brief Direct-drive mode switch — implementation.
 *
 * @details Engagement is gated by speed; disengagement is always immediate.
 *   See switch_direct_drive.h for the full behavioural contract.
 *****************************************************************************/

#include "switch_direct_drive.h"

#include <core/system/combus/combus_access.h>   // combus_set_digital


// =============================================================================
// 1. SWITCH UPDATE
// =============================================================================

/** @brief Direct-drive switch update — see switch_direct_drive.h for contract. */
void switch_direct_drive_update(const DirectDriveSwitchCfg* cfg,
                                DirectDriveSwitchState*     state,
                                ComBus&                     bus,
                                ChanOwner                   owner)
{
    // --- 0. Read button — isDrived guard prevents stale pre-connection state. ---
    const uint8_t btnIdx = static_cast<uint8_t>(cfg->btnCh);
    const bool    btnNow = bus.digitalBus[btnIdx].isDrived
                        && bus.digitalBus[btnIdx].value;
    const bool    rising = btnNow && !state->prevBtn;
    state->prevBtn = btnNow;

    // --- 1. Toggle on rising edge. ---
    if (rising) {
        if (!state->active) {
              // Engage: speed guard.
            const uint16_t spd = bus.analogBus[static_cast<uint8_t>(cfg->speedCh)].value;
            if (cfg->maxEngageSpd == 0u || spd <= cfg->maxEngageSpd) {
                state->active = true;
            }
        } else {
              // Disengage: always immediate.
            state->active = false;
        }
    }

    // --- 2. Write output every cycle. ---
    combus_set_digital(bus, cfg->outCh, state->active, owner);
}

// EOF switch_direct_drive.cpp
