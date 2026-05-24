/******************************************************************************
 * @file  sim_bypass.cpp
 * @brief SimProc function — conditional bypass gate (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Reads one digital channel; claims the pipeline when HIGH.
 *****************************************************************************/

#include "sim_bypass.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Conditional bypass gate — see sim_bypass.h for full contract. */
void sim_bypass_fn(SimProc* proc, uint16_t& /*value*/, ComBus& bus, bool& claimed, ChanOwner /*chanOwner*/)
{
    const SimBypassCfg* cfg = static_cast<const SimBypassCfg*>(proc->cfg);
    if (bus.digitalBus[static_cast<uint8_t>(cfg->condCh)].value)
        claimed = true;
    // value untouched — sim_channel_update writes raw inCh to outCh when claimed.
}

// EOF sim_bypass.cpp
