/******************************************************************************
 * @file proc_failsafe_reset.cpp
 * @brief Failsafe processor — pivot reset at the start of each cycle.
 *
 * @details Resets `failsafeBus.active` to `false` at the beginning of
 *   each cycle. The function is the single, explicit owner of the
 *   pivot reset — `failsafe_update()` does not perform any inline
 *   reset.
 *
 *   Contract (WIP §5):
 *     - reads no channel;
 *     - does not modify `value` (pass-through);
 *     - never sets `claimed`;
 *     - has no RunLevel / machine / environment logic;
 *     - has no reaction logic.
 *
 *   The processor participates in the main Failsafe chain
 *   `kFailsafeChain` as the first entry (reset → contributors).
 *****************************************************************************/

#include "proc_failsafe_reset.h"

#include "failsafe.h"  // failsafeBus


// =============================================================================
// 1. PROCESSOR IMPLEMENTATION
// =============================================================================

/**
 * @brief Clears the Failsafe pivot at cycle start.
 *
 * @param proc     CbProc descriptor — unused (pass-through).
 *                 `inCh` = nullopt, `outCh` = nullopt, `cfg` = nullptr.
 * @param value    Pipeline value — not modified.
 * @param claimed  Never set to `true`.
 */
void proc_failsafe_reset_fn(CbProc* proc, uint16_t& value, bool& claimed)
{
    (void)proc;     // No descriptor data consumed.
    (void)value;    // Pass-through.
    (void)claimed;  // Never claimed.

    // WIP: explicit pivot reset — single owner of this assignment.
    failsafeBus.active = false;
}

// EOF proc_failsafe_reset.cpp
