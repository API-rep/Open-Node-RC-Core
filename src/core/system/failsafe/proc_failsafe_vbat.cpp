/******************************************************************************
 * @file  proc_failsafe_vbat.cpp
 * @brief Failsafe processor — VBAT contributor (implementation).
 *
 * @details Aggregates `DigitalComBusID::FAILSAFE_VBAT` into the central
 *   Failsafe pivot (`failsafeBus.active`) and forces the sub-combus back
 *   to fault every cycle (proof-of-life pattern, WIP §5).
 *
 *   Contract (WIP §6):
 *     - reads `proc->inValue` (FAILSAFE_VBAT state);
 *     - sets `failsafeBus.active = true` if `inValue != 0`;
 *     - writes `proc->outValue = 1` so the runner commits
 *       `proc->outCh` (= FAILSAFE_VBAT) back to fault;
 *     - never sets `claimed`;
 *     - has no RunLevel / machine / environment logic;
 *     - has no reaction logic.
 *****************************************************************************/

#include "proc_failsafe_vbat.h"

#include "failsafe.h"  // failsafeBus


// =============================================================================
// 1. PROCESSOR IMPLEMENTATION
// =============================================================================

/**
 * @brief VBAT contributor — see proc_failsafe_vbat.h for full contract.
 *
 * @param proc     CbProc descriptor — reads `inValue`, writes `outValue`.
 * @param value    Pipeline value — pass-through.
 * @param claimed  Never claimed.
 */
void proc_failsafe_vbat_fn(CbProc* proc, uint16_t& value, bool& claimed)
{
    (void)value;    // Pass-through.
    (void)claimed;  // Never claimed.

    // --- 1. Latch the central pivot when the sub-combus reports fault ------
    if (proc->inValue != 0u) {
        failsafeBus.active = true;
    }

    // --- 2. Always reset the sub-combus to fault (consume + guard) ----------
    // The runner will commit proc->outValue to proc->outCh after this fn.
    proc->outValue = 1u;
}

// EOF proc_failsafe_vbat.cpp
