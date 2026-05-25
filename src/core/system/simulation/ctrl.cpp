/*!****************************************************************************
 * @file  ctrl.cpp
 * @brief CtrlChannel dispatcher — runner implementation.
 *****************************************************************************/

#include "ctrl.h"

#include <struct/ctrl_struct.h>   // CtrlChannel, CtrlProc
#include <struct/combus_struct.h>  // ComBus, ChanOwner


// =============================================================================
// 1. PUBLIC API
// =============================================================================

void ctrl_update(CtrlChannel* channels, uint8_t count, ComBus& bus, ChanOwner owner)
{
    if (!channels || count == 0u) return;

    for (uint8_t ch = 0u; ch < count; ++ch) {
        CtrlChannel& chan = channels[ch];

        bool value   = false;
        bool claimed = false;

        //  Dispatch proc chain — bus I/O is handled by ctrl_read_fn / ctrl_write_fn.
        //  Any proc may set claimed = true to abort the remaining chain.
        for (uint8_t p = 0u; p < chan.procCount; ++p) {
            if (claimed) break;
            if (chan.procs[p].fn) {
                chan.procs[p].fn(&chan.procs[p], value, bus, claimed, owner);
            }
        }
    }
}

// EOF ctrl.cpp

// EOF ctrl.cpp
