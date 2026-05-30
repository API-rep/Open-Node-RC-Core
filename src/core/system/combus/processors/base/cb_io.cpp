/******************************************************************************
 * @file  cb_io.cpp
 * @brief CbProc functions — pipeline source and sink (implementation).
 *****************************************************************************/

#include "cb_io.h"


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/** @brief Pipeline source — see cb_io.h for full contract. */
void cb_in_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*owner*/)
{
    value = proc->inValue;
}


/** @brief Pipeline sink — see cb_io.h for full contract. */
void cb_out_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*owner*/)
{
    proc->outValue = value;
}

// EOF cb_io.cpp
