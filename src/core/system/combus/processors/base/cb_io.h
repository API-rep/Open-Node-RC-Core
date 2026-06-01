/******************************************************************************
 * @file  cb_io.h
 * @brief CbProc functions — pipeline source and sink.
 *
 * @details Two utility procs for explicit pipeline I/O within a CbProc array.
 *   Primary chain I/O is handled by `CbChain::inCh` / `outCh` in the runner —
 *   these functions are kept for read-modify-write patterns or nested sub-chains.
 *
 *   - `cb_in_fn`  — seeds the pipeline `value` from `proc->inValue`
 *                   (pre-filled by the runner from `proc.inCh`).
 *   - `cb_out_fn` — copies the pipeline `value` to `proc->outValue`
 *                   (committed by the runner to `proc.outCh`).
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
