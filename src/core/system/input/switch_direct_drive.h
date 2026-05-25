/******************************************************************************
 * @file  switch_direct_drive.h
 * @brief Direct-drive mode switch — conditional engagement via speed guard.
 *
 * @details Monitors a raw button channel and a speed channel.
 *   On a rising edge of `btnCh`:
 *     - Engage  (active → true):  only when `bus[speedCh] <= maxEngageSpd`
 *                                 (or `maxEngageSpd == 0`, no guard).
 *     - Disengage (active → false): always immediate, regardless of speed.
 *   Writes `outCh` with the current state every cycle.
 *
 *   The `isDrived` flag on `btnCh` is checked — if the input module has not
 *   written the channel yet (PS4 not connected), the button is treated as LOW,
 *   preventing a spurious engage on startup.
 *****************************************************************************/
#pragma once

#include <struct/combus_struct.h>              // ComBus, ChanOwner
#include <core/config/machines/combus_ids.h>  // AnalogComBusID, DigitalComBusID
#include <stdint.h>


// =============================================================================
// 1. CONFIGURATION AND STATE
// =============================================================================

/// Static configuration for the direct-drive mode switch.
struct DirectDriveSwitchCfg {
    DigitalComBusID btnCh;        ///< Raw button channel (e.g. DIRECT_DRIVE_BTN). `isDrived` guard applied.
    DigitalComBusID outCh;        ///< Output channel written every cycle (e.g. DIRECT_DRIVE).
    AnalogComBusID  speedCh;      ///< Speed channel checked on engagement (e.g. RPM_BUS, 0..maxRpm).
    uint16_t        maxEngageSpd; ///< Max speed value allowing engagement. 0 = no guard.
};

/// Mutable runtime state — zero-init is valid (`active = false`, `prevBtn = false`).
struct DirectDriveSwitchState {
    bool active;    ///< Current DIRECT_DRIVE state.
    bool prevBtn;   ///< Previous button state — rising-edge detection.
};


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Update the direct-drive switch — call once per main-loop cycle.
 *
 * @param cfg   Static configuration (flash).
 * @param state Mutable runtime state (RAM).
 * @param bus   ComBus — read `btnCh` and `speedCh`; write `outCh`.
 * @param owner Channel-owner identity for `combus_set_digital`.
 */
void switch_direct_drive_update(const DirectDriveSwitchCfg* cfg,
                                DirectDriveSwitchState*     state,
                                ComBus&                     bus,
                                ChanOwner                   owner);

// EOF switch_direct_drive.h
