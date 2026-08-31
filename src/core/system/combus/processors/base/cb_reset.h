/******************************************************************************
 * @file  cb_reset.h
 * @brief CbProc function — unconditional pipeline reset.
 *
 * @details `cb_reset_fn()` is a generic CbProcFn that forces `value = 0`
 *   unconditionally. It is used as the first processor of pipelines that
 *   need a per-cycle reset (e.g. the FAILSAFE chain).
 *
 *   Behaviour:
 *     - reads no channel;
 *     - writes no channel;
 *     - sets `value = 0`;
 *     - never sets `claimed`.
 *
 *   Typical declaration (in a chain array):
 *   @code
 *     { .name = "reset",
 *       .fn   = cb_reset_fn,
 *       // inCh / outCh omitted = nullopt
 *     },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Unconditional pipeline reset — assigned to `CbProc::fn`.
 *
 * @details Matches the `CbProcFn` signature.  Sets `value = 0` and leaves
 *   `claimed` unchanged.  No bus I/O is performed.
 *
 * @param proc     CbProc descriptor (unused — no cfg/state).
 * @param value    Out: forced to 0.
 * @param claimed  Unchanged.
 */
void cb_reset_fn(CbProc* proc, uint16_t& value, bool& claimed);

// EOF cb_reset.h
