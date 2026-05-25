/******************************************************************************
 * @file  cb_ramp.h
 * @brief CbProc function — single-axis inertia ramp (hydraulics, steering).
 *
 * @details `cb_ramp_fn()` implements an asymmetric inertia ramp for use as a
 *   CbProc within a CbChain pipeline.
 *
 *   The function:
 *   - Reads `value` (target already seeded from CbChain::inCh before proc 0).
 *   - Advances `state->currentPos` toward the target by `accelSteps` (moving
 *     away from neutral) or `brakeSteps` (returning toward neutral) each
 *     `rampTimeMs` tick.
 *   - Writes the filtered position back to `value` — CbChain owns the
 *     final bus write after all procs complete.
 *   - Does NOT set `claimed = true`, so downstream procs may still run.
 *
 *   Multi-instance: one CbProc entry per axis.  Each entry must have its own
 *   `CbRampState` instance (zero-initialised — first call snaps to CbusNeutral).
 *
 *   Typical declaration (in vehicle proc_config.cpp):
 *   @code
 *     static CbRampState gSteerRampState {};
 *     static CbProc kSteeringProcs[] = {
 *         { .name="ramp", .fn=cb_ramp_fn, .cfg=&kMyRampCfg, .state=&gSteerRampState }
 *     };
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>                         // CbProc, CbProcFn, ChanOwner
#include <struct/combus/processors/motion/cb_ramp_struct.h>    // CbRampCfg, CbRampState


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Asymmetric inertia ramp — assigned to `CbProc::fn`.
 *
 * @details Matches the `CbProcFn` signature.  Self-inits on first call when
 *   `state->lastUpdateMs == 0` (snaps to CbusNeutral, resets the timer).
 *   Reads the target from `value`; writes the filtered position back.
 *   Does not set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `CbRampCfg*`,
 *                `state` cast to `CbRampState*`.  Neither may be nullptr.
 * @param value   Channel value (in/out) — target on entry; ramped position on return.
 * @param claimed Not modified — downstream procs continue after this one.
 * @param chainOwner  Not used — ramp is self-contained (reads/writes through `value` only).
 */
void cb_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_ramp.h
