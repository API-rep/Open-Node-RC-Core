/******************************************************************************
 * @file proc_failsafe_reset.cpp
 * @brief Failsafe processor — pivot reset at the start of each cycle.
 *
 * @details Stub at step 1: the body is empty. The actual reset of
 *   `failsafeBus.active` is deferred to step 2 (WIP §12.2). Once the
 *   reset processor is wired into `kFailsafeChain`, the body will
 *   perform the in-place reset:
 *   @code
 *   (void)proc;
 *   (void)value;
 *   (void)claimed;
 *   failsafeBus.active = false;
 *   @endcode
 *****************************************************************************/

#include "proc_failsafe_reset.h"

#include "failsafe.h"  // failsafeBus (used at step 2)


// =============================================================================
// 1. PROCESSOR IMPLEMENTATION
// =============================================================================

/**
 * @brief No-op stub at step 1.
 *
 * @details The pivot `failsafeBus.active` is not touched here yet.
 *   The reset is performed exclusively by the processor function once
 *   step 2 wires it into `kFailsafeChain`. Until then, the
 *   `failsafe_update()` orchestrator is itself a no-op.
 */
void proc_failsafe_reset_fn(CbProc* proc, uint16_t& value, bool& claimed)
{
    (void)proc;     // Ignored at step 1.
    (void)value;    // Pass-through.
    (void)claimed;  // Never set.

    // WIP: step 2 — failsafeBus.active = false;
}

// EOF proc_failsafe_reset.cpp
