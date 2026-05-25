/******************************************************************************
 * @file  sim_bypass.h
 * @brief SimProc function — conditional bypass gate.
 *
 * @details `sim_bypass_fn()` implements a bypass gate for a SimChannel pipeline.
 *
 *   When `SimBypassCfg::condCh` is HIGH, `value` is written to `outCh` and
 *   `claimed` is set to `true`, skipping all downstream processors.
 *   When `condCh` is LOW, the function is always a no-op.
 *
 *   No runtime state: `SimProc::state` must be `nullptr`.
 *
 *   Typical declaration (in vehicle sim_config.cpp):
 *   @code
 *     static constexpr SimBypassCfg kMyBypass {
 *         .condCh = DigitalComBusID::DIRECT_DRIVE,
 *         .outCh  = AnalogComBusID::RPM_BUS,
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
void sim_bypass_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& claimed, ChanOwner chanOwner);

// EOF sim_bypass.h
