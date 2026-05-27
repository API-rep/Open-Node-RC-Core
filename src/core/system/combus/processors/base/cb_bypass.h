/******************************************************************************
 * @file  cb_bypass.h
 * @brief CbProc function — conditional bypass gate.
 *
 * @details `cb_bypass_fn()` implements a bypass gate for a CbChain pipeline.
 *
 *   When `proc->inValue[0]` (digital) is nonzero, `claimed` is set to
 *   `true`, skipping all downstream processors.
 *
 *   Optional force value: when `CbProc::cfg` points to `CbBypassCfg` with
 *   `forceValue` set, `value` is overwritten on claim (e.g. force gear=1).
 *   When `cfg = nullptr` or `forceValue = nullopt`, `value` passes through.
 *
 *   Typical declarations (in vehicle proc_config.cpp):
 *   @code
 *     // Pure bypass — value passthrough
 *     { .name = "bypass",
 *       .inCh = { DigitalComBusID::DIRECT_DRIVE },
 *       .fn   = cb_bypass_fn,
 *       // cfg omitted = nullptr → value unchanged
 *     },
 *
 *     // Bypass + force value
 *     static constexpr CbBypassCfg kGearDirectCfg { .forceValue = 1u };
 *     { .name = "direct-claim",
 *       .inCh = { DigitalComBusID::DIRECT_DRIVE },
 *       .fn   = cb_bypass_fn,
 *       .cfg  = &kGearDirectCfg,  // force gear=1 on claim
 *     },
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
 *
 *   If `cfg` points to `CbBypassCfg` with `forceValue` set, `value` is
 *   overwritten on claim.  Otherwise, `value` passes through unchanged.
 *
 * @param proc    CbProc descriptor.
 *                `inCh[0]` = condition digital channel (set in config array).
 *                `cfg` = optional `CbBypassCfg*` (nullptr = passthrough mode).
 *                `state` = unused (nullptr).
 * @param value   In: current value.  Out: unchanged OR forceValue (if cfg set).
 * @param claimed Set to `true` when inValue[0] != 0; unchanged otherwise.
 */
void cb_bypass_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_bypass.h
