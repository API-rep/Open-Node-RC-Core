/*!****************************************************************************
 * @file  ctrl_io.cpp
 * @brief Bus I/O CtrlProcFns — read and write procs implementation.
 *****************************************************************************/

#include "ctrl_io.h"

#include <struct/ctrl_struct.h>                  // CtrlProc, CtrlReadCfg, CtrlWriteCfg
#include <struct/combus_struct.h>                 // ComBus
#include <core/system/combus/combus_access.h>     // combus_set_digital


// =============================================================================
// 1. PROC IMPLEMENTATIONS
// =============================================================================

void ctrl_read_fn(CtrlProc* proc, bool& value, ComBus& bus,
                  bool& /*claimed*/, ChanOwner /*owner*/)
{
    const auto* cfg = static_cast<const CtrlReadCfg*>(proc->cfg);
    const uint8_t idx = static_cast<uint8_t>(cfg->inCh);
    value = bus.digitalBus[idx].isDrived && bus.digitalBus[idx].value;
}

void ctrl_write_fn(CtrlProc* proc, bool& value, ComBus& bus,
                   bool& /*claimed*/, ChanOwner owner)
{
    const auto* cfg = static_cast<const CtrlWriteCfg*>(proc->cfg);
    combus_set_digital(bus, cfg->outCh, value, owner);
}

// EOF ctrl_io.cpp
