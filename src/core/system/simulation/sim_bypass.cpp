/******************************************************************************
 * @file  sim_bypass.cpp
 * @brief SimProc function — conditional bypass gate (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Reads one digital secondary input; claims the pipeline when nonzero.
 *****************************************************************************/

#include "sim_bypass.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Conditional bypass gate — see sim_bypass.h for full contract. */
void sim_bypass_fn(SimProc* proc, uint16_t& /*value*/, bool& claimed, ChanOwner /*chanOwner*/)
{
    // secInCh[0] = condition channel (digital). Runner pre-reads into secInValue[0].
    if (proc->secInValue[0] != 0u) {
        claimed = true;  // Runner always post-writes value to optOutCh; no explicit write needed.
    }
}

// EOF sim_bypass.cpp
