/******************************************************************************
 * @file proc_failsafe_reset.h
 * @brief Failsafe processor — pivot reset at the start of each cycle.
 *
 * @details CbProcFn processor that resets `failsafeBus.active` to
 *   `false` at the beginning of each cycle. Its business logic is
 *   unconditional (never sets `claimed = true`) and it does not
 *   consult any ComBus channel.
 *
 *   The processor is the single owner of the pivot reset: no other
 *   site in the Failsafe module performs an inline reset.
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
 *   Sole effect: `failsafeBus.active = false`.
 *
 *   Pipeline configuration in the chain array:
 *     - `inCh`     = nullopt (no channel read);
 *     - `outCh`    = nullopt (no channel write);
 *     - `cfg`      = nullptr (no config);
 *     - `state`    = nullptr (no state);
 *     - `dynCfg`   = nullptr (no runtime override).
 *
 * @param proc     CbProc descriptor — unused (pass-through).
 * @param value    Pipeline value — not modified.
 * @param claimed  Never set to `true`.
 */
void proc_failsafe_reset_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF proc_failsafe_reset.h
