/******************************************************************************
 * @file  cb_or.cpp
 * @brief CbProc function — OR-guard (latch value + side-output).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Reads `proc->inValue` (sub-combus state), latches `value` to 1
 *   when nonzero, and writes `proc->outValue = 1` so the runner
 *   commits `proc->outCh` back to fault.  Never claims the chain.
 *****************************************************************************/

#include "cb_or.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief OR-guard processor — see cb_or.h for full contract. */
void cb_or_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/)
{
    // --- 1. Latch the top-level value if the sub-combus reports fault -----
    if (proc->inValue != 0u) {
        value = 1u;
    }

    // --- 2. Always reset the sub-combus to fault (consume + guard) ---------
    // The runner will commit proc->outValue to proc->outCh after this fn.
    proc->outValue = 1u;
}

// EOF cb_or.cpp
