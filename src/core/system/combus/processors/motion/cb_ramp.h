/******************************************************************************
 * @file  cb_ramp.h
 * @brief CbProc functions — inertia ramps (bipolar and unipolar variants).
 *
 * @details Two variants:
 *   - `cb_sym_ramp_fn()` — bipolar, operates around CbusNeutral [0..CbusMaxVal].
 *     "Symmetric" refers to the bipolar axis, not to the accel/brake rates
 *     (see `CbRampCfg::accelDownSteps` for asymmetric reverse acceleration).
 *   - `cb_uni_ramp_fn()` — unipolar, operates on magnitude [0..CbusMaxVal]
 *     where 0 = stopped and CbusMaxVal = full speed.  Direction is not encoded
 *     in the value — the caller must track it separately (e.g. DRIVE_STATE_BUS).
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

/**
 * @brief Unipolar inertia ramp — magnitude domain [0..CbusMaxVal], 0 = stopped.
 *
 * @details Variant of `cb_sym_ramp_fn` for channels where direction has already
 *   been extracted.  Differences vs bipolar variant:
 *   - Self-init snaps to 0 (stopped), not CbusNeutral.
 *   - `neutralBand`: target snapped to 0 when `target <= neutralBand`
 *     (small deadzone near zero prevents creep on stick release).
 *   - No `accelDownSteps` path — magnitude has no negative direction concept.
 *   - `extBrakeSteps` / `extAccelSteps` modifiers still apply.
 *   - Does not set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `CbRampCfg*`,
 *                `state` cast to `CbRampState*`.  Neither may be nullptr.
 * @param value   Channel value (in/out) — magnitude target on entry; ramped magnitude on return.
 * @param claimed Not modified.
 * @param chainOwner  Not used.
 */
void cb_uni_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF cb_ramp.h
