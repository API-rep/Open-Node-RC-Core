/*!****************************************************************************
 * @file  ctrl_speed_gate.cpp
 * @brief Speed-gate CtrlProcFn — suppresses engagement above a speed threshold.
 *****************************************************************************/

#include "ctrl_speed_gate.h"

#include <struct/ctrl_struct.h>    // CtrlProc, CtrlSpeedGateCfg


// =============================================================================
// 1. PROC IMPLEMENTATION
// =============================================================================

void ctrl_speed_gate_fn(CtrlProc* proc, uint16_t& value,
                        bool& /*claimed*/, ChanOwner /*owner*/)
{
    const auto* cfg = static_cast<const CtrlSpeedGateCfg*>(proc->cfg);

    //  No guard configured, or button not pressed — nothing to gate.
    if (!cfg || cfg->maxEngageSpd == 0u || value == 0u) return;

    //  secInCh[0] = RPM_BUS (analog): current speed.
    //  secInCh[1] = DIRECT_DRIVE (digital): currently active flag.
    //  Gate bypasses when already active: disengagement is always allowed.
    if (proc->secInValue[1] != 0u) return;

    //  Not active and button pressed — check speed before allowing engagement.
    if (proc->secInValue[0] > cfg->maxEngageSpd) {
        value = 0u;  // suppress: vehicle too fast to engage
    }
}

// EOF ctrl_speed_gate.cpp
