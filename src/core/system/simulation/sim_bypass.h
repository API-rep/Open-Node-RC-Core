/******************************************************************************
 * @file  sim_bypass.h
 * @brief SimProc function — conditional bypass gate.
 *
 * @details `sim_bypass_fn()` implements a bypass gate for a SimChannel pipeline.
 *
 *   When `proc->secInValue[0]` (digital) is nonzero, `claimed` is set to
 *   `true`, skipping all downstream processors.  The runner always post-writes
 *   `value` to `ch.optOutCh`, so the raw input passes through to the output.
 *   When the condition is false, the function is a no-op.
 *
 *   No config struct needed: `SimProc::cfg` must be `nullptr`.
 *   No runtime state: `SimProc::state` must be `nullptr`.
 *
 *   Typical declaration (in vehicle sim_config.cpp):
 *   @code
 *     { .name="bypass",
 *       .secInCh = { DigitalComBusID::DIRECT_DRIVE },
 *       .fn      = sim_bypass_fn,
 *       .cfg     = nullptr,
 *       .state   = nullptr },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>  // SimProc, CbProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Conditional bypass gate — assigned to `SimProc::fn`.
 *
 * @details Matches the `CbProcFn` (`SimProcFn`) signature.
 *   Sets `claimed = true` when `proc->secInValue[0]` is nonzero,
 *   causing all downstream processors to be skipped this cycle.
 *   Does not modify `value` — raw optInCh value passes through to optOutCh.
 *
 * @param proc    CbProc descriptor — `cfg` and `state` are unused (nullptr).
 *                `secInCh[0]` = condition digital channel (set in config array).
 * @param value   Not modified — runner always writes to optOutCh after chain.
 * @param claimed Set to `true` when secInValue[0] != 0; unchanged otherwise.
 */
void sim_bypass_fn(SimProc* proc, uint16_t& value, bool& claimed, ChanOwner chanOwner);

// EOF sim_bypass.h
