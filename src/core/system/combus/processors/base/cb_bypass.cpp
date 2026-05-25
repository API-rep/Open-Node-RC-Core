/******************************************************************************
 * @file  cb_bypass.cpp
 * @brief CbProc function — conditional bypass gate (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Reads one digital secondary input; claims the pipeline when nonzero.
 *****************************************************************************/

#include "cb_bypass.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Conditional bypass gate — see cb_bypass.h for full contract. */
void cb_bypass_fn(CbProc* proc, uint16_t& /*value*/, bool& claimed, ChanOwner /*chainOwner*/)
{
    // secInCh[0] = condition channel (digital). Runner pre-reads into secInValue[0].
    if (proc->secInValue[0] != 0u) {
        claimed = true;  // Runner always post-writes value to optOutCh; no explicit write needed.
    }
}

// EOF cb_bypass.cpp
