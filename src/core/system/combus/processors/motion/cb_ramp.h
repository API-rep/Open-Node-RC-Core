/******************************************************************************
 * @file  cb_ramp.h
 * @brief CbProc function — symmetric (bipolar) inertia ramp around CbusNeutral.
 *
 * @details `cb_sym_ramp_fn()` implements an inertia ramp that operates
 *   symmetrically in BOTH directions around CbusNeutral on a bipolar ComBus
 *   channel [0..CbusMaxVal].  "Symmetric" refers to the bipolar axis, not to
 *   the accel/brake rates — those may differ (see `CbRampCfg::accelDownSteps`).
 *
 *   The function:
 *   - Reads `value` (target already seeded from CbChain::inCh before proc 0).
 *   - Advances `state->currentPos` toward the target by `accelSteps` (moving
 *     away from neutral) or `brakeSteps` (returning toward neutral) each
 *     `rampTimeMs` tick.  Applies `extBrakeSteps` on top when set externally.
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
 *         { .name="ramp", .fn=cb_sym_ramp_fn, .cfg=&kMyRampCfg, .state=&gSteerRampState }
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
 * @brief Bipolar inertia ramp — assigned to `CbProc::fn`.
 *
 * @details Matches the `CbProcFn` signature.  Self-inits on first call when
 *   `state->lastUpdateMs == 0` (snaps to CbusNeutral, resets the timer).
 *   Reads the target from `value`; writes the filtered position back.
 *   Accel and brake step rates may be different (see `CbRampCfg`).
 *   Does not set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `CbRampCfg*`,
 *                `state` cast to `CbRampState*`.  Neither may be nullptr.
 * @param value   Channel value (in/out) — target on entry; ramped position on return.
 * @param claimed Not modified — downstream procs continue after this one.
 * @param chainOwner  Not used — ramp is self-contained (reads/writes through `value` only).
 */
void cb_sym_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_ramp.h
