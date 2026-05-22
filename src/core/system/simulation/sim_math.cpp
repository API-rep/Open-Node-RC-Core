/*!****************************************************************************
 * @file  sim_math.cpp
 * @brief SimProc functions — generic arithmetic transforms.
 *****************************************************************************/

#include "sim_math.h"

#include <core/system/combus/combus_res.h>   // CbusNeutral


// =============================================================================
// 1. SIMPROC FUNCTIONS
// =============================================================================

/** @brief Signed center deviation \u2014 see sim_math.h for contract. */
void sim_center_fn(SimProc* /*proc*/, uint16_t& value, ComBus& /*bus*/, bool& /*claimed*/)
{
    // Reinterpret as signed: (int16_t)(value - CbusNeutral), packed back in uint16_t.
    // Positive side (FWD): value > CbusNeutral  \u2192 small positive int16.
    // Negative side (REV): value < CbusNeutral  \u2192 two's complement (e.g. -300 = 0xFED4).
    value = static_cast<uint16_t>(static_cast<int16_t>(value)
                                - static_cast<int16_t>(CbusNeutral));
}

/** @brief Absolute value with optional digital side effect — see sim_math.h for contract. */
void sim_abs_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& /*claimed*/)
{
    const int16_t sv = static_cast<int16_t>(value);

    // --- Optional digital side effect (sign: HIGH = FWD, LOW = REV) -----------
    if (proc->optOutDCh.has_value()) {
        bus.digitalBus[static_cast<uint8_t>(*proc->optOutDCh)].value = (sv >= 0);
    }

    // --- Absolute value --------------------------------------------------------
    value = static_cast<uint16_t>(sv >= 0 ? sv : -sv);
}

/** @brief Linear scale \u2014 see sim_math.h for contract. */
void sim_scale_fn(SimProc* proc, uint16_t& value, ComBus& /*bus*/, bool& /*claimed*/)
{
    const SimScaleCfg* cfg = static_cast<const SimScaleCfg*>(proc->cfg);

    value = static_cast<uint16_t>(
        static_cast<int32_t>(value)       * static_cast<int32_t>(cfg->outMax)
      / static_cast<int32_t>(cfg->inMax));
}


// EOF sim_math.cpp
