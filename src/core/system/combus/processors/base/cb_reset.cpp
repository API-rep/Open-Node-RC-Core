/******************************************************************************
 * @file  cb_reset.cpp
 * @brief CbProc function — unconditional pipeline reset (implementation).
 *
 * @details Pure ComBus processor — no hardware calls, no µs domain.
 *   Forces `value = 0` unconditionally.  No bus I/O.
 *****************************************************************************/

#include "cb_reset.h"


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Unconditional pipeline reset — see cb_reset.h for full contract. */
void cb_reset_fn(CbProc* /*proc*/, uint16_t& value, bool& /*claimed*/)
{
    value = 0u;
}

// EOF cb_reset.cpp
