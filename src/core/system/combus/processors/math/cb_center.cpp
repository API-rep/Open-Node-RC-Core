/******************************************************************************
 * @file  cb_center.cpp
 * @brief CbProc function — signed center deviation (value − neutral).
 *****************************************************************************/

#include "cb_center.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Signed center deviation — see cb_center.h for contract. */
void cb_center_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const CbCenterCfg* cfg = static_cast<const CbCenterCfg*>(proc->cfg);
    value = static_cast<uint16_t>(static_cast<int16_t>(value)
                                - static_cast<int16_t>(cfg->neutral));
}

// EOF cb_center.cpp
