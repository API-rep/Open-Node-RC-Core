/*!****************************************************************************
 * @file    ctrl_config.cpp
 * @brief   Volvo A60H Bruder — CtrlChain pipeline configuration.
 *
 * @details Defines the CtrlChain array consumed by ctrl_update() in the
 *   machine main loop.
 *
 *   Channel pipelines (optInCh → procs → optOutCh):
 *     CTRL_DIRECT_DRIVE : DIRECT_DRIVE_BTN → speed_gate(RPM_BUS,DIRECT_DRIVE), toggle → DIRECT_DRIVE
 *****************************************************************************/

#include "ctrl_config.h"

#include <struct/ctrl_struct.h>                       // CtrlChain, CtrlProc, CtrlSpeedGateCfg, CtrlToggleState
#include <struct/combus_struct.h>                     // makeChanOwner, ComBusOwner
#include <core/config/machines/combus_types.h>        // DigitalComBusID, AnalogComBusID
#include <core/system/simulation/ctrl_speed_gate.h>   // ctrl_speed_gate_fn
#include <core/system/simulation/ctrl_toggle.h>       // ctrl_toggle_fn


// =============================================================================
// 1. SPEED GATE CONFIG
// =============================================================================

//  Engage guard: ~10 % of kHeavy3 maxRpm scale (0..2100).
//  secInCh[0] = RPM_BUS (speed); secInCh[1] = DIRECT_DRIVE (currently active).
static constexpr CtrlSpeedGateCfg kDirectDriveGateCfg {
    .maxEngageSpd = 200u,
};


// =============================================================================
// 2. TOGGLE STATE
// =============================================================================

static CtrlToggleState gDirectDriveToggleState {};


// =============================================================================
// 3. PROC ARRAYS
// =============================================================================

static CtrlProc kDirectDriveProcs[] = {
    { .name      = "speed_gate",
      .secInCh   = { AnalogComBusID::RPM_BUS, DigitalComBusID::DIRECT_DRIVE },
      .fn        = ctrl_speed_gate_fn,
      .cfg       = &kDirectDriveGateCfg,
      .state     = nullptr,
    },
    { .name  = "toggle",
      .fn    = ctrl_toggle_fn,
      .cfg   = nullptr,
      .state = &gDirectDriveToggleState,
    },
};


// =============================================================================
// 4. CHANNEL ARRAY
// =============================================================================

CtrlChain kCtrlChains[] = {
    { .name      = "direct_drive",
      .optInCh   = DigitalComBusID::DIRECT_DRIVE_BTN,
      .optOutCh  = DigitalComBusID::DIRECT_DRIVE,
      .procs     = kDirectDriveProcs,
      .procCount = static_cast<uint8_t>(std::size(kDirectDriveProcs)),
      .chainOwner = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM),
    },
};

const uint8_t kCtrlChainCount = sizeof(kCtrlChains) / sizeof(CtrlChain);

// EOF ctrl_config.cpp
