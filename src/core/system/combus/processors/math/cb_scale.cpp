/******************************************************************************
 * @file  cb_scale.cpp
 * @brief CbProc function — linear domain rescale.
 *****************************************************************************/

#include "cb_scale.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Linear scale — see cb_scale.h for contract. */
void cb_scale_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const CbScaleCfg* cfg = static_cast<const CbScaleCfg*>(proc->cfg);
    value = static_cast<uint16_t>(
        static_cast<int32_t>(value)       * static_cast<int32_t>(cfg->outMax)
      / static_cast<int32_t>(cfg->inMax));
}

// EOF cb_scale.cpp
