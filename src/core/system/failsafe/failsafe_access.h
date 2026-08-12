/******************************************************************************
 * @file failsafe_access.h
 * @brief Failsafe read-only façade — access without depending on failsafe.h.
 *
 * @details Allows other modules to consult the Failsafe state without
 *   including `failsafe.h` directly or the full chain. Minimal
 *   header, no dependency towards `machines/`.
 *****************************************************************************/
#pragma once

#include "failsafe.h"  // FailsafeBus, failsafeBus

/**
 * @brief Returns the current state of the Failsafe pivot.
 *
 * @details Inline read-only accessor. No public setter is exposed:
 *   writing to `failsafeBus.active` is strictly internal to the
 *   Failsafe module.
 *
 * @return true if at least one fault was detected during the current cycle.
 */
inline bool failsafe_is_active()
{
    return failsafeBus.active;
}

// EOF failsafe_access.h
