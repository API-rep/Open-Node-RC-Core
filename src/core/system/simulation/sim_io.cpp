/******************************************************************************
 * @file  sim_io.cpp
 * @brief SimProc functions — ComBus read and write endpoints (implementation).
 *****************************************************************************/

#include "sim_io.h"
#include "core/system/combus/combus_access.h"   // combus_set_analog
#include <core/system/combus/combus_res.h>      // CbusMaxVal (digital → analog conversion)


// =============================================================================
// 1. PROC FUNCTIONS
// =============================================================================

/** @brief Seeds pipeline value from ComBus — see sim_io.h for contract. */
void sim_read_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& /*claimed*/, ChanOwner /*chanOwner*/)
{
    // --- Analog source (priority) --------------------------------------------
    if (proc->optInCh.has_value()) {
        value = bus.analogBus[static_cast<uint8_t>(proc->optInCh.value())].value;
        return;
    }
    // --- Digital source (silent conversion: false → 0, true → CbusMaxVal) ----
    if (proc->optInDCh.has_value()) {
        value = bus.digitalBus[static_cast<uint8_t>(proc->optInDCh.value())].value
                    ? static_cast<uint16_t>(CbusMaxVal) : 0u;
    }
}

/** @brief Writes pipeline value to ComBus — see sim_io.h for contract. */
void sim_write_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& /*claimed*/, ChanOwner chanOwner)
{
    combus_set_analog(bus, proc->optOutCh.value(), value, chanOwner);
}

// EOF sim_io.cpp
