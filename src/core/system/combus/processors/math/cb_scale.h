/******************************************************************************
 * @file  cb_scale.h
 * @brief CbProc function — linear domain rescale.
 *
 * @details `cb_scale_fn()` rescales `value` from [0..inMax] to [0..outMax]:
 *   `value = value × outMax / inMax`
 *   No clamping.  cfg = &CbScaleCfg, state = nullptr.
 *   Does NOT set `claimed`.
 *
 *   Typical use: last proc in a THROTTLE_BUS → ESC_RPM_BUS pipeline:
 *   @code
 *     { .fn = cb_center_fn, .cfg = &kCenter }
 *     { .fn = cb_abs_fn,    .cfg = nullptr  }
 *     { .fn = cb_scale_fn,  .cfg = &kScale  }   // maps [0..CbusNeutral] → [0..maxRpm]
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                         // CbProc, CbProcFn, ChanOwner
#include <struct/combus/processors/math/cb_scale_struct.h>    // CbScaleCfg


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Linear scale — assigned to `CbProc::fn`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `const CbScaleCfg*`.
 * @param value   In/Out: value to rescale.
 * @param claimed Not modified.
 * @param chainOwner  Not used.
 */
void cb_scale_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_scale.h
