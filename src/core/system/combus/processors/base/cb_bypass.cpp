/******************************************************************************
 * @file  cb_bypass.cpp
 * @brief CbProc function — conditional bypass gate (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Reads one digital secondary input; claims the pipeline when nonzero.
 *   Optionally overwrites `value` on claim when `cfg->forceValue` is set.
 *****************************************************************************/

#include "cb_bypass.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Conditional bypass gate — see cb_bypass.h for full contract. */
void cb_bypass_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner /*chainOwner*/)
{
    // inCh = condition channel (digital). Runner pre-reads into inValue.
    if (proc->inValue != 0u) {
        // Optional force value — overwrite on claim.
        if (proc->cfg != nullptr) {
            const CbBypassCfg* cfg = static_cast<const CbBypassCfg*>(proc->cfg);
            if (cfg->forceValue.has_value()) {
                value = cfg->forceValue.value();
            }
        }
        claimed = true;
    }
}

// EOF cb_bypass.cpp
