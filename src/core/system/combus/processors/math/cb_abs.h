/******************************************************************************
 * @file  cb_abs.h
 * @brief CbProc function — absolute value of a signed-packed uint16_t.
 *
 * @details `cb_abs_fn()` interprets `value` as two's complement int16_t
 *   (as produced by `cb_center_fn`), computes its absolute value, and writes
 *   the magnitude back.  cfg = nullptr, state = nullptr.
 *   Does NOT set `claimed`.
 *
 *   Sign side effect (direction flag) via:
 *   - `proc->outValue` = 1 (FWD/positive) or 0 (REV/negative)
 *   - `proc->outCh` = digital channel; runner commits after fn
 *     (nullopt = no side effect).
 *
 *   Typical use: second proc in a THROTTLE_BUS → ESC_RPM_BUS pipeline:
 *   @code
 *     { .fn = cb_center_fn, .cfg = &kCenter }
 *     { .fn = cb_abs_fn,    .cfg = nullptr,
 *       .outCh = DigitalComBusID::ESC_REVERSE_BUS }  // HIGH=FWD LOW=REV
 *     { .fn = cb_scale_fn,  .cfg = &kScale  }
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>   // CbProc, CbProcFn, ChanOwner


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Absolute value with sign side effect — assigned to `CbProc::fn`.
 *
 * @param proc    CbProc descriptor — cfg = nullptr.
 *                `outCh` = optional digital channel for sign side effect.
 * @param value   In: signed-packed int16.  Out: magnitude [0..CbusNeutral].
 * @param claimed Not modified.
 * @param chainOwner  Not used.
 */
void cb_abs_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_abs.h
