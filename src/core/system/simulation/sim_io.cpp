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
    if (!proc->optInCh.has_value()) return;
    if (std::holds_alternative<DigitalComBusID>(*proc->optInCh)) {
        value = bus.digitalBus[static_cast<uint8_t>(std::get<DigitalComBusID>(*proc->optInCh))].value
                    ? static_cast<uint16_t>(CbusMaxVal) : 0u;
    } else {
        value = bus.analogBus[static_cast<uint8_t>(std::get<AnalogComBusID>(*proc->optInCh))].value;
    }
}

/** @brief Writes pipeline value to ComBus — see sim_io.h for contract. */
void sim_write_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& /*claimed*/, ChanOwner chanOwner)
{
    if (!proc->optOutCh.has_value()) return;
    if (std::holds_alternative<DigitalComBusID>(*proc->optOutCh)) {
        bus.digitalBus[static_cast<uint8_t>(std::get<DigitalComBusID>(*proc->optOutCh))].value = (value != 0u);
    } else {
        combus_set_analog(bus, std::get<AnalogComBusID>(*proc->optOutCh), value, chanOwner);
    }
}

// EOF sim_io.cpp
