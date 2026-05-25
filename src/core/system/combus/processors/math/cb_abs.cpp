/******************************************************************************
 * @file  cb_abs.cpp
 * @brief CbProc function — absolute value of a signed-packed uint16_t.
 *****************************************************************************/

#include "cb_abs.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Absolute value with sign side effect — see cb_abs.h for contract. */
void cb_abs_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const int16_t sv   = static_cast<int16_t>(value);
    proc->secOutValue  = static_cast<uint16_t>(sv >= 0 ? 1u : 0u);  // runner commits to optSecOutCh
    value              = static_cast<uint16_t>(sv >= 0 ? sv : -sv);
}

// EOF cb_abs.cpp
