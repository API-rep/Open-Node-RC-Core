/******************************************************************************
 * @file  cb_io.h
 * @brief CbProc functions — pipeline source and sink.
 *
 * @details Two terminal procs that replace the old CbChain primary I/O fields.
 *   Place them as the first and last entries in every CbProc array:
 *
 *   - `cb_in_fn`  — seeds the pipeline `value` from `proc->inValue`.
 *                   Set `proc.inCh` to the source channel; the runner
 *                   pre-reads it into `proc->inValue` before the call.
 *   - `cb_out_fn` — copies the pipeline `value` to `proc->outValue`.
 *                   Set `proc.outCh` to the destination channel; the runner
 *                   commits `proc->outValue` to the bus after the call.
 *                   The runner always executes the last proc regardless of
 *                   `claimed`, so `cb_out_fn` commits even after a bypass.
 *
 *   Typical array layout:
 *   @code
 *     { .name="in",  .inCh=AnalogComBusID::THROTTLE_BUS, .fn=cb_in_fn  },
 *     // ... intermediate processing procs ...
 *     { .name="out", .outCh=AnalogComBusID::RPM_BUS,     .fn=cb_out_fn },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn, ChanOwner


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/// @brief Pipeline source — copies `proc->inValue` into the pipeline `value`.
void cb_in_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner owner);

/// @brief Pipeline sink — copies the pipeline `value` to `proc->outValue`.
void cb_out_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner owner);


// EOF cb_io.h
