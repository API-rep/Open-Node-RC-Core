/******************************************************************************
 * @file  cb_or.h
 * @brief CbProc function — OR-guard (latch value + side-output).
 *
 * @details `cb_or_fn()` is a generic CbProcFn that implements the
 *   "consume + guard" pattern used to aggregate sub-combus sources
 *   (e.g. `FAILSAFE_VBAT`) into a top-level latch (e.g. `FAILSAFE`).
 *
 *   Behaviour:
 *     1. Reads `proc->inValue` (injected by the runner from
 *        `proc->inCh` — the sub-combus state).
 *     2. If `inValue != 0`, forces `value = 1` (latch the top-level
 *        to fault).
 *     3. ALWAYS writes `proc->outValue = 1` so the runner commits
 *        `proc->outCh` (= same sub-combus) back to fault.  This is the
 *        "guard" half: the sub-combus is consumed and reset to fault
 *        every cycle, regardless of the top-level latch state.
 *
 *   The proc never sets `claimed = true` so the chain continues —
 *   the runner skips procs when claimed, and the "all contributors
 *   must run every cycle" invariant would otherwise be broken.
 *
 *   Typical declaration (in a chain array):
 *   @code
 *     { .name = "or-vbat",
 *       .inCh  = DigitalComBusID::FAILSAFE_VBAT,
 *       .outCh = DigitalComBusID::FAILSAFE_VBAT,
 *       .fn    = cb_or_fn,
 *     },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief OR-guard processor — assigned to `CbProc::fn`.
 *
 * @details Matches the `CbProcFn` signature.  Reads `proc->inValue`
 *   (sub-combus state), latches `value` to 1 when nonzero, and writes
 *   `proc->outValue = 1` so the runner commits `proc->outCh` (= same
 *   sub-combus) back to fault.
 *
 *   Never claims the chain.
 *
 * @param proc     CbProc descriptor.
 *                 `inCh`  = sub-combus to read.
 *                 `outCh` = sub-combus to write back (= same channel).
 *                 `cfg` / `state` = unused (nullptr).
 * @param value    In: current pipeline value.  Out: forced to 1 if
 *                 `proc->inValue != 0`, unchanged otherwise.
 * @param claimed  Unchanged (always false).
 */
void cb_or_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF cb_or.h
