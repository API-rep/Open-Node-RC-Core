/*!****************************************************************************
 * @file  ctrl.cpp
 * @brief CbChain dispatcher for ctrl layer — runner implementation.
 *****************************************************************************/

#include "ctrl.h"

#include <struct/combus_struct.h>              // ComBus
#include <core/system/combus/combus_access.h>  // combus_set_analog, combus_set_digital


// =============================================================================
// 0. INTERNAL HELPERS  (shared pattern with sim.cpp)
// =============================================================================

using ChanOpt = std::optional<std::variant<AnalogComBusID, DigitalComBusID>>;

static uint16_t cbRead(const ComBus& bus, const ChanOpt& ch, bool isDrivedGuard)
{
    if (!ch.has_value()) return 0u;
    if (std::holds_alternative<AnalogComBusID>(*ch)) {
        return bus.analogBus[static_cast<uint8_t>(std::get<AnalogComBusID>(*ch))].value;
    }
    const uint8_t idx = static_cast<uint8_t>(std::get<DigitalComBusID>(*ch));
    if (isDrivedGuard && !bus.digitalBus[idx].isDrived) return 0u;
    return bus.digitalBus[idx].value ? 1u : 0u;
}

static void cbWrite(ComBus& bus, const ChanOpt& ch, uint16_t value, ChanOwner owner)
{
    if (!ch.has_value()) return;
    if (std::holds_alternative<AnalogComBusID>(*ch)) {
        combus_set_analog(bus, std::get<AnalogComBusID>(*ch), value, owner);
    } else {
        combus_set_digital(bus, std::get<DigitalComBusID>(*ch), value != 0u, owner);
    }
}


// =============================================================================
// 1. PUBLIC API
// =============================================================================

void ctrl_update(CbChain* channels, uint8_t count, ComBus& bus)
{
    if (!channels || count == 0u) return;

    for (uint8_t ch = 0u; ch < count; ++ch) {
        CbChain& chan = channels[ch];

        // --- Pre-read primary input (isDrived guard for raw operator inputs) --
        uint16_t value   = cbRead(bus, chan.optInCh, /*isDrivedGuard=*/true);
        bool     claimed = false;

        // --- Proc chain -------------------------------------------------------
        for (uint8_t p = 0u; p < chan.procCount && !claimed; ++p) {
            CbProc& proc = chan.procs[p];
            if (proc.fn == nullptr) continue;

            //  Inject secondary inputs (no isDrived guard — internal channels).
            for (uint8_t i = 0u; i < 3u; ++i) {
                proc.inValue[i] = cbRead(bus, proc.inCh[i], /*isDrivedGuard=*/false);
            }

            //  Call proc fn.
            proc.fn(&proc, value, claimed, chan.chainOwner);

            //  Commit proc output.
            cbWrite(bus, proc.outCh, proc.outValue, chan.chainOwner);
        }

        // --- Post-write primary output (always) -------------------------------
        cbWrite(bus, chan.outCh, value, chan.chainOwner);
    }
}

// EOF ctrl.cpp
