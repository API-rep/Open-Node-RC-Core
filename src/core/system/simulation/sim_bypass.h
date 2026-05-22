/******************************************************************************
 * @file  sim_bypass.h
 * @brief SimProc function — conditional bypass gate.
 *
 * @details `sim_bypass_fn()` implements an early-exit gate for use as the
 *   first SimProc in a SimChannel pipeline.
 *
 *   When `SimBypassCfg::condCh` digital channel is HIGH, `claimed` is set
 *   to `true`, skipping all downstream processors for this cycle.  `value`
 *   is left unchanged — the raw `inCh` input passes directly to `outCh`.
 *
 *   When `condCh` is LOW, the function is a no-op and downstream procs
 *   continue normally.
 *
 *   No runtime state: `SimProc::state` must be `nullptr`.
 *   Typically placed at `simProc[0]` of any channel supporting bypass mode.
 *
 *   Typical declaration (in vehicle sim_config.cpp):
 *   @code
 *     static constexpr SimBypassCfg kMyBypass {
 *         .condCh = DigitalComBusID::DIRECT_DRIVE
 *     };
 *     static SimProc kThrottleProcs[] = {
 *         { .name="bypass", .fn=sim_bypass_fn, .cfg=&kMyBypass, .state=nullptr },
 *         // ... other procs follow
 *     };
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // SimProc, SimBypassCfg, SimProcFn
#include <struct/combus_struct.h>       // ComBus


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Conditional bypass gate — assigned to `SimProc::fn`.
 *
 * @details Matches the `SimProcFn` signature.
 *   Sets `claimed = true` when `cfg->condCh` digital channel is HIGH,
 *   causing all downstream processors to be skipped this cycle.
 *   Does not modify `value` — raw inCh value passes through to outCh.
 *
 * @param proc    SimProc descriptor — `cfg` cast to `const SimBypassCfg*`.
 *                `state` is unused (must be nullptr).
 * @param value   Not modified — raw inCh value passes through to outCh.
 * @param claimed Set to `true` when condCh is HIGH; unchanged otherwise.
 * @param bus     Read-only — reads `digitalBus[condCh].value`.
 */
void sim_bypass_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed);

// EOF sim_bypass.h
