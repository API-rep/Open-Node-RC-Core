/*!****************************************************************************
 * @file  ctrl.cpp
 * @brief CtrlChannel dispatcher — runner implementation.
 *****************************************************************************/

#include "ctrl.h"

#include <struct/ctrl_struct.h>              // CtrlChannel, CtrlProc
#include <struct/combus_struct.h>            // ComBus, ChanOwner
#include <core/system/combus/combus_access.h>  // combus_set_digital


// =============================================================================
// 1. PUBLIC API
// =============================================================================

void ctrl_update(CtrlChannel* channels, uint8_t count, ComBus& bus, ChanOwner owner)
{
    if (!channels || count == 0u) return;

    for (uint8_t ch = 0u; ch < count; ++ch) {
        CtrlChannel& chan = channels[ch];

        //  isDrived guard: skip channel entirely if the input source has not written
        //  to inCh this cycle (button is stale / no operator input).
        const uint8_t inIdx = static_cast<uint8_t>(chan.inCh);
        if (!bus.digitalBus[inIdx].isDrived) continue;

        bool value   = bus.digitalBus[inIdx].value;
        bool claimed = false;

        //  Dispatch proc chain.
        for (uint8_t p = 0u; p < chan.procCount; ++p) {
            if (chan.procs[p].fn) {
                chan.procs[p].fn(&chan.procs[p], value, bus, claimed, owner);
            }
        }

        //  Write outCh unless a proc already claimed the write.
        if (!claimed) {
            combus_set_digital(bus, chan.outCh, value, owner);
        }
    }
}

// EOF ctrl.cpp
