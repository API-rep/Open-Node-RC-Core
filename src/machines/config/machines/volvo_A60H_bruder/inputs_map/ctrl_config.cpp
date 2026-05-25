/*!****************************************************************************
 * @file    ctrl_config.cpp
 * @brief   Volvo A60H Bruder — CtrlChannel pipeline configuration.
 *
 * @details Defines the CtrlChannel array consumed by ctrl_update() in the
 *   machine main loop.
 *
 *   Channel pipelines:
 *     CTRL_DIRECT_DRIVE : read(DIRECT_DRIVE_BTN) → speed_gate(RPM_BUS,≤200) → toggle → write(DIRECT_DRIVE)
 *****************************************************************************/

#include "ctrl_config.h"

#include <struct/ctrl_struct.h>                       // CtrlChannel, CtrlProc, cfg/state types
#include <core/config/machines/combus_types.h>        // DigitalComBusID, AnalogComBusID
#include <core/system/simulation/ctrl_io.h>           // ctrl_read_fn, ctrl_write_fn
#include <core/system/simulation/ctrl_speed_gate.h>   // ctrl_speed_gate_fn
#include <core/system/simulation/ctrl_toggle.h>       // ctrl_toggle_fn


// =============================================================================
// 1. I/O CONFIGS
// =============================================================================

static constexpr CtrlReadCfg  kDirectDriveReadCfg  { .inCh  = DigitalComBusID::DIRECT_DRIVE_BTN };
static constexpr CtrlWriteCfg kDirectDriveWriteCfg { .outCh = DigitalComBusID::DIRECT_DRIVE };


// =============================================================================
// 2. SPEED GATE CONFIG
// =============================================================================

//  Engage guard: ~10 % of kHeavy3 maxRpm scale (0..2100).
//  Bypass when DIRECT_DRIVE is already active (disengagement always allowed).
static constexpr CtrlSpeedGateCfg kDirectDriveGateCfg {
    .speedCh      = AnalogComBusID::RPM_BUS,
    .maxEngageSpd = 200u,
    .activeCh     = DigitalComBusID::DIRECT_DRIVE,
};


// =============================================================================
// 3. TOGGLE STATE
// =============================================================================

static CtrlToggleState gDirectDriveToggleState {};


// =============================================================================
// 4. PROC ARRAYS
// =============================================================================

static CtrlProc kDirectDriveProcs[] = {
    { "read",       ctrl_read_fn,       &kDirectDriveReadCfg,  nullptr                    },
    { "speed_gate", ctrl_speed_gate_fn, &kDirectDriveGateCfg,  nullptr                    },
    { "toggle",     ctrl_toggle_fn,     nullptr,               &gDirectDriveToggleState   },
    { "write",      ctrl_write_fn,      &kDirectDriveWriteCfg, nullptr                    },
};


// =============================================================================
// 5. CHANNEL ARRAY
// =============================================================================

CtrlChannel kCtrlChannels[] = {
    { "direct_drive", kDirectDriveProcs, 4u },
};

const uint8_t kCtrlChannelCount = sizeof(kCtrlChannels) / sizeof(CtrlChannel);

// EOF ctrl_config.cpp
