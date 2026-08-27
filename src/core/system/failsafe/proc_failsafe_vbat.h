/******************************************************************************
 * @file  proc_failsafe_vbat.h
 * @brief Failsafe processor — VBAT contributor (reads + resets FAILSAFE_VBAT).
 *
 * @details CbProcFn processor that aggregates the VBAT sub-ComBus
 *   (`DigitalComBusID::FAILSAFE_VBAT`) into the central Failsafe pivot
 *   (`failsafeBus.active`).
 *
 *   Behaviour (WIP §6 "guarded execution / local short-circuit"):
 *     1. Reads `proc->inValue` (the sub-combus state, injected by the
 *        runner from `proc->inCh`).
 *     2. If `inValue != 0` (cell voltage is below cutoff) → sets
 *        `failsafeBus.active = true` (latch).
 *     3. ALWAYS writes `proc->outValue = 1` (fault) so the runner
 *        commits `proc->outCh` (= same sub-combus) back to fault —
 *        the sub-combus is consumed and reset every cycle
 *        (proof-of-life pattern, WIP §5).
 *
 *   The processor never sets `claimed = true` so the chain continues
 *   (WIP §6: every contributor must run every cycle).
 *
 *   Pipeline configuration in the chain array (see `failsafe_chain.cpp`):
 *     - `inCh`   = DigitalComBusID::FAILSAFE_VBAT
 *     - `outCh`  = DigitalComBusID::FAILSAFE_VBAT
 *     - `cfg`    = nullptr (no config)
 *     - `state`  = nullptr (no state)
 *
 *   This processor is added to `kFailsafeProcs[]` under
 *   `#if defined(HAS_VBAT_FAILSAFE)`.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn


// =============================================================================
// 1. PROCESSOR FUNCTION
// =============================================================================

/**
 * @brief VBAT contributor — aggregates FAILSAFE_VBAT into the pivot.
 *
 * @details Standard `CbProcFn` signature. Reads `proc->inValue` (the
 *   FAILSAFE_VBAT sub-combus state injected by the runner), latches
 *   `failsafeBus.active` to `true` when the cell is low, and writes
 *   `proc->outValue = 1` so the runner commits `proc->outCh` back
 *   to fault.
 *
 *   Never sets `claimed`.
 *
 * @param proc     CbProc descriptor.
 *                 `inCh`  = `DigitalComBusID::FAILSAFE_VBAT`.
 *                 `outCh` = `DigitalComBusID::FAILSAFE_VBAT`.
 *                 `cfg` / `state` = unused (nullptr).
 * @param value    Pipeline value — not modified.
 * @param claimed  Never set to `true`.
 */
void proc_failsafe_vbat_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF proc_failsafe_vbat.h
