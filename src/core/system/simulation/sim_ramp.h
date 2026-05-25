/******************************************************************************
 * @file  sim_ramp.h
 * @brief CbProc function — single-axis inertia ramp (hydraulics, steering).
 *
 * @details `sim_ramp_fn()` implements an asymmetric inertia ramp for use as a
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
 *   `SimRampState` instance (zero-initialised — first call snaps to CbusNeutral).
 *
 *   Typical declaration (in vehicle sim_config.cpp):
 *   @code
 *     static SimRampState gSteerRampState {};
 *     static CbProc kSteeringProcs[] = {
 *         { .name="ramp", .fn=sim_ramp_fn, .cfg=&kMyRampCfg, .state=&gSteerRampState }
 *     };
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // CbProc, SimRampCfg, SimRampState, CbProcFn
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Asymmetric inertia ramp — assigned to `CbProc::fn`.
 *
 * @details Matches the `CbProcFn` signature.  Self-inits on first call when
 *   `state->currentPos == 0` (snaps to CbusNeutral, resets the timer).
 *   Reads the target from `value`; writes the filtered position back.
 *   Does not set `claimed`.
 *
 * @param proc    CbProc descriptor — `cfg` cast to `SimRampCfg*`,
 *                `state` cast to `SimRampState*`.  Neither may be nullptr.
 * @param value   Channel value (in/out) — target on entry; ramped position on return.
 * @param claimed Not modified — downstream procs continue after this one.
 * @param bus     Not used — ramp is self-contained (reads/writes through `value` only).
 */
void sim_ramp_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner);

// EOF sim_ramp.h
