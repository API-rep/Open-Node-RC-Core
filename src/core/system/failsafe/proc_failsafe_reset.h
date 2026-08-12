/******************************************************************************
 * @file proc_failsafe_reset.h
 * @brief Failsafe processor — pivot reset at the start of each cycle.
 *
 * @details CbProcFn processor that resets `failsafeBus.active` to
 *   `false` at the beginning of each cycle. Its business logic is
 *   unconditional (never sets `claimed = true`) and it does not
 *   consult any ComBus channel.
 *
 *   At step 1, this processor is a **no-op stub**: the pivot is
 *   not yet reset by this function. The reset will be performed
 *   exclusively by this processor at step 2 (WIP §12.2), once it is
 *   wired into `kFailsafeChain`.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn


// =============================================================================
// 1. PROCESSOR FUNCTION
// =============================================================================

/**
 * @brief Reset processor — clears the Failsafe pivot at cycle start.
 *
 * @details Standard `CbProcFn` signature. Reads no channel, does not
 *   modify `value`, never sets `claimed`.
 *
 *   Stub at step 1: empty body. The effective reset is deferred to
 *   step 2, when the processor becomes the first entry of
 *   `kFailsafeChain`.
 *
 * @param proc     CbProc descriptor (ignored at step 1).
 * @param value    Pipeline value (pass-through, not modified).
 * @param claimed  Never set to `true`.
 */
void proc_failsafe_reset_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF proc_failsafe_reset.h
