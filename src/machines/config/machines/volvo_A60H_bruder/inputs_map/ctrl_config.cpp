/*!****************************************************************************
 * @file    ctrl_config.cpp
 * @brief   Volvo A60H Bruder — CtrlChannel pipeline configuration.
 *
 * @details Defines the CtrlChannel array consumed by ctrl_update() in the
 *   machine main loop.
 *
 *   Channel pipelines:
 *     CTRL_DIRECT_DRIVE : DIRECT_DRIVE_BTN → [toggle (speed-gated)] → DIRECT_DRIVE
 *****************************************************************************/

#include "ctrl_config.h"

#include <struct/ctrl_struct.h>                // CtrlChannel, CtrlProc, CtrlToggleCfg, CtrlToggleState
#include <core/config/machines/combus_types.h> // DigitalComBusID, AnalogComBusID (+ using namespace)
#include <core/system/simulation/ctrl_toggle.h>  // ctrl_toggle_fn


// =============================================================================
// 1. TOGGLE CONFIGS AND STATES
// =============================================================================

//  Direct-drive engage guard: ~10 % of kHeavy3 maxRpm scale (0..2100).
static constexpr CtrlToggleCfg kDirectDriveToggleCfg {
    .speedCh      = AnalogComBusID::RPM_BUS,
    .maxEngageSpd = 200u,
};

static CtrlToggleState gDirectDriveToggleState {};


// =============================================================================
// 2. PROC ARRAYS
// =============================================================================

static CtrlProc kDirectDriveProcs[] = {
    { "toggle", ctrl_toggle_fn, &kDirectDriveToggleCfg, &gDirectDriveToggleState },
};


// =============================================================================
// 3. CHANNEL ARRAY
// =============================================================================

CtrlChannel kCtrlChannels[] = {
    {
        .name      = "direct_drive",
        .inCh      = DigitalComBusID::DIRECT_DRIVE_BTN,
        .outCh     = DigitalComBusID::DIRECT_DRIVE,
        .procs     = kDirectDriveProcs,
        .procCount = 1u,
    },
};

const uint8_t kCtrlChannelCount = sizeof(kCtrlChannels) / sizeof(CtrlChannel);

// EOF ctrl_config.cpp
