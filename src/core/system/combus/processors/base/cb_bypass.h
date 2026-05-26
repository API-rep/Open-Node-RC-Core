/******************************************************************************
 * @file  cb_bypass.h
 * @brief CbProc function — conditional bypass gate.
 *
 * @details `cb_bypass_fn()` implements a bypass gate for a CbChain pipeline.
 *
 *   When `proc->inValue[0]` (digital) is nonzero, `claimed` is set to
 *   `true`, skipping all downstream processors.  The runner always post-writes
 *   `value` to `ch.outCh`, so the raw input passes through to the output.
 *   When the condition is false, the function is a no-op.
 *
 *   No config struct needed: `CbProc::cfg` must be `nullptr`.
 *   No runtime state: `CbProc::state` must be `nullptr`.
 *
 *   Typical declaration (in vehicle proc_config.cpp):
 *   @code
 *     { .name    = "bypass",
 *       .inCh = { DigitalComBusID::DIRECT_DRIVE },
 *       .fn      = cb_bypass_fn,
 *       .cfg     = nullptr,
 *       .state   = nullptr },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                  // CbProc, CbProcFn, ChanOwner
#include <struct/combus/processors/base/cb_bypass_struct.h>  // CbBypassCfg


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Conditional bypass gate — assigned to `CbProc::fn`.
 *
 * @details Matches the `CbProcFn` signature.
 *   Sets `claimed = true` when `proc->inValue[0]` is nonzero,
 *   causing all downstream processors to be skipped this cycle.
 *   Does not modify `value` — raw optInCh value passes through to outCh.
 *
 * @param proc    CbProc descriptor — `cfg` and `state` are unused (nullptr).
 *                `inCh[0]` = condition digital channel (set in config array).
 * @param value   Not modified — runner always writes to outCh after chain.
 * @param claimed Set to `true` when inValue[0] != 0; unchanged otherwise.
 */
void cb_bypass_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_bypass.h
