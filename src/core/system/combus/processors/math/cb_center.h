/******************************************************************************
 * @file  cb_center.h
 * @brief CbProc function — signed center deviation (value − neutral).
 *
 * @details `cb_center_fn()` recenters a raw unsigned ComBus value around a
 *   neutral point, producing a signed-packed uint16_t:
 *   `value = (uint16_t)(int16_t)(value − cfg->neutral)`
 *
 *   Positive result = FWD side, negative = REV side (two's complement).
 *   Does NOT set `claimed`.  cfg = &CbCenterCfg, state = nullptr.
 *
 *   Typical use: first proc in a THROTTLE_BUS → RPM_BUS pipeline:
 *   @code
 *     { .fn = cb_center_fn, .cfg = &kCenter }   // signed deviation
 *     { .fn = cb_abs_fn,    .cfg = nullptr  }   // magnitude
 *     { .fn = cb_scale_fn,  .cfg = &kScale  }   // rescale to RPM
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                          // CbProc, CbProcFn, ChanOwner
#include <struct/combus/processors/math/cb_center_struct.h>    // CbCenterCfg


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Signed center deviation — assigned to `CbProc::fn`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `const CbCenterCfg*`.
 * @param value   In: unsigned ComBus [0..CbusMaxVal].  Out: signed packed int16.
 * @param claimed Not modified.
 * @param chainOwner  Not used.
 */
void cb_center_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_center.h
