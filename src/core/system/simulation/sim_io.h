/******************************************************************************
 * @file  sim_io.h
 * @brief SimProc functions — ComBus read and write endpoints.
 *
 * @details Two symmetrical SimProc entries that bookend every SimChannel
 *   pipeline:
 *
 *   `sim_read_fn`  — placed as proc[0]: seeds `value` from `proc->optInCh`.
 *                    Analog source: reads `analogBus[id].value` directly.
 *                    Digital source (`isDigital == true`): converts false → 0, true → CbusMaxVal.
 *   `sim_write_fn` — placed as proc[N]: writes `value` to `proc->optOutCh`.
 *                    Analog target: calls `combus_set_analog()`.
 *                    Digital target (`isDigital == true`): writes `value != 0` to `digitalBus`.
 *
 *   This makes every pipeline self-documenting: the array alone shows
 *   the full data flow from read to write, without needing to look at
 *   the enclosing `SimChannel` struct.
 *
 *   `sim_bypass_fn` (see sim_bypass.h) short-circuits the pipeline
 *   including `sim_write_fn`: it writes directly to `cfg->outCh` before
 *   claiming, so the output channel is always written exactly once per
 *   cycle regardless of the bypass state.
 *
 *   Typical pipeline layout:
 *   @code
 *     { .name="read",  .optInCh  = FOO_BUS, .fn=sim_read_fn,  .cfg=nullptr },   // proc 0
 *     { .name="bypass",             .fn=sim_bypass_fn,  .cfg=&kBypassCfg },   // optional
 *     { .name="...", ... },                                                    // transforms
 *     { .name="write", .optOutCh = BAR_BUS, .fn=sim_write_fn, .cfg=nullptr },   // proc N
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/simulation_struct.h>   // SimProc, SimProcFn
#include <struct/combus_struct.h>       // ComBus
#include <defs/defs.h>                  // ChanOwner


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Seeds pipeline `value` from a ComBus analog channel.
 *
 * @details Matches the `SimProcFn` signature.  Reads
 *   `bus.analogBus[cfg->ch].value` and assigns it to `value`.
 *   Does not set `claimed`.
 *   Place as the first proc in every SimChannel pipeline.
 *
 * @param proc       SimProc descriptor — `proc->optInCh` must have a value.
 *                   Analog: reads `analogBus[id].value`.
 *                   Digital (`isDigital`): false → 0, true → CbusMaxVal.
 *                   `cfg` and `state` are unused (must be nullptr).
 * @param value      Out: overwritten with the channel value (digital: false→0, true→CbusMaxVal).
 * @param bus        ComBus — source of the read.
 * @param claimed    Not modified.
 * @param chanOwner  Not used.
 */
void sim_read_fn(SimProc* proc, uint16_t& value, ComBus& bus,
                 bool& claimed, ChanOwner chanOwner);

/**
 * @brief Writes pipeline `value` to a ComBus analog channel.
 *
 * @details Matches the `SimProcFn` signature.  Calls `combus_set_analog()`
 *   with `chanOwner` from the runner.  Does not set `claimed`.
 *   Place as the last proc in every SimChannel pipeline.
 *   Not reached when a bypass proc has already set `claimed = true`.
 *
 * @param proc       SimProc descriptor — `proc->optOutCh` must have a value.
 *                   Analog: calls `combus_set_analog()` with `chanOwner`.
 *                   Digital (`isDigital`): writes `value != 0` to `digitalBus`.
 *                   `cfg` and `state` are unused (must be nullptr).
 * @param value      In: final pipeline value to write.
 * @param bus        ComBus — target of the write.
 * @param claimed    Not modified.
 * @param chanOwner  Passed to `combus_set_analog()` as the write identity.
 */
void sim_write_fn(SimProc* proc, uint16_t& value, ComBus& bus,
                  bool& claimed, ChanOwner chanOwner);

// EOF sim_io.h
