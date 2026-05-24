/******************************************************************************
 * @file  sim_ramp.h
 * @brief SimProc function — single-axis inertia ramp (hydraulics, steering).
 *
 * @details `sim_ramp_fn()` implements an asymmetric inertia ramp for use as a
 *   SimProc within a SimChannel pipeline.
 *
 *   The function:
 *   - Reads `value` (target already seeded from SimChannel::inCh before proc 0).
 *   - Advances `state->currentPos` toward the target by `accelSteps` (moving
 *     away from neutral) or `brakeSteps` (returning toward neutral) each
 *     `rampTimeMs` tick.
 *   - Writes the filtered position back to `value` — SimChannel owns the
 *     final bus write after all procs complete.
 *   - Does NOT set `claimed = true`, so downstream procs may still run.
 *
 *   Multi-instance: one SimProc entry per axis.  Each entry must have its own
 *   `SimRampState` instance (zero-initialised — first call snaps to CbusNeutral).
 *
 *   Typical declaration (in vehicle sim_config.cpp):
 *   @code
 *     static SimRampState gSteerRampState {};
 *     static SimProc kSteeringProcs[] = {
 *         { .name="ramp", .fn=sim_ramp_fn, .cfg=&kMyRampCfg, .state=&gSteerRampState }
 *     };
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // SimProc, SimRampCfg, SimRampState, SimProcFn
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Asymmetric inertia ramp — assigned to `SimProc::fn`.
 *
 * @details Matches the `SimProcFn` signature.  Self-inits on first call when
 *   `state->currentPos == 0` (snaps to CbusNeutral, resets the timer).
 *   Reads the target from `value`; writes the filtered position back.
 *   Does not set `claimed`.
 *
 * @param proc    SimProc descriptor — `cfg` cast to `SimRampCfg*`,
 *                `state` cast to `SimRampState*`.  Neither may be nullptr.
 * @param value   Channel value (in/out) — target on entry; ramped position on return.
 * @param claimed Not modified — downstream procs continue after this one.
 * @param bus     Not used — ramp is self-contained (reads/writes through `value` only).
 */
void sim_ramp_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed, ChanOwner chanOwner);

// EOF sim_ramp.h
