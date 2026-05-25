/*!****************************************************************************
 * @file  ctrl_speed_gate.cpp
 * @brief Speed-gate CtrlProcFn — suppresses engagement above a speed threshold.
 *****************************************************************************/

#include "ctrl_speed_gate.h"

#include <struct/ctrl_struct.h>    // CtrlProc, CtrlSpeedGateCfg
#include <struct/combus_struct.h>  // ComBus


// =============================================================================
// 1. PROC IMPLEMENTATION
// =============================================================================

void ctrl_speed_gate_fn(CtrlProc* proc, bool& value, ComBus& bus,
                        bool& /*claimed*/, ChanOwner /*owner*/)
{
    const auto* cfg = static_cast<const CtrlSpeedGateCfg*>(proc->cfg);

    //  No guard configured, or button not pressed — nothing to gate.
    if (!cfg || cfg->maxEngageSpd == 0u || !value) return;

    //  Gate bypasses when already active: disengagement is always allowed.
    const bool currentlyActive =
        bus.digitalBus[static_cast<uint8_t>(cfg->activeCh)].value;
    if (currentlyActive) return;

    //  Not active and button pressed — check speed before allowing engagement.
    const uint16_t spd = bus.analogBus[static_cast<uint8_t>(cfg->speedCh)].value;
    if (spd > cfg->maxEngageSpd) {
        value = false;  // suppress: vehicle too fast to engage
    }
}

// EOF ctrl_speed_gate.cpp
