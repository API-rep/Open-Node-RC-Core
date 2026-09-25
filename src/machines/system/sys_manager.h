/******************************************************************************
 * @file sys_manager.h
 * @brief System-level tick orchestrator — single entry point per loop cycle.
 *
 * @details Centralises all recurring system services that must run once per
 *   loop, in a deterministic order, before application logic:
 *
 *   1. Input acquisition: core `input_refresh()` then machine
 *      `input_update(bus)` — each backend writes its OWN *_LINK_LOST
 *      contributor (FS1 redesign, see doc/failsafe_module.md §12.5).
 *   2. Battery update via `vbat_update()` — ADC sensing, sliding average,
 *      low-bat detection, and re-arm of `DigitalComBusID::FAILSAFE_VBAT`.
 *   3. Failsafe update via `failsafe_update(bus)` — runs the FAILSAFE
 *      processor chain (reset + contributors), publishes the aggregated
 *      `DigitalComBusID::FAILSAFE` channel.
 *   4. Chantier 12.5 — `remote_link_fallback_update(bus)` (gated by
 *      `HAS_REMOTE_LINK_LOST_FALLBACK`) — if REMOTE_LINK_LOST just rose,
 *      forces RUNLEVEL = IDLE.
 *
 *   **Chantier 12.6 (cleanup):** the historical open-drain
 *   `bus.isDrived` / `bus.isNotDrived` flag has been REMOVED from the
 *   `ComBus` struct (see `combus_defs.h`).  Link health is now monitored
 *   per-channel by independent contributors (no shared state, no ordering
 *   pitfalls).  `sys_manager_reset(ComBus&)` is now a NO-OP kept for
 *   source-level compatibility with out-of-tree callers (sound node etc.).
 *
 *   **Why this file lives under machines/ and not core/:**
 *   Step 1 calls this machine's own `input_update(bus)` (the input device ->
 *   ComBus mapping — see machines/system/input/input_update.h), which is
 *   machine-specific by nature. Keeping sys_manager here avoids a core file
 *   depending on a machine path. The trade-off: each machine owns its own
 *   copy of this orchestrator. If that duplication becomes painful across
 *   several machines, consider factoring the generic part (vbat / failsafe
 *   / link-fallback) back into core behind a registered input hook — ask
 *   if/when that's worth revisiting.
 *
 *   Machine node: call `sys_manager_update()` once as the first statement
 *   in `loop()`.
 *
 *   Sound node: no equivalent needed — link health is monitored per-link
 *   by the UART RX path which publishes `UART_LINK_LOST` directly.  See
 *   `combus_sound_interpreter.cpp` for the wiring.
 *
 *   The aggregated Failsafe state is published on
 *   `comBus.digitalBus[DigitalComBusID::FAILSAFE]` by the Failsafe chain
 *   (see `doc/failsafe_module.md`).
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>


// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Full system tick — call ONCE per loop, BEFORE application logic.
 *
 * @details Executes in order:
 *   1. `input_refresh()` (core) — physical acquisition.
 *      `input_update(bus)` (machine) — input -> ComBus mapping.
 *   2. `vbat_update()` — battery sensing + re-arm of `FAILSAFE_VBAT`.
 *   3. `failsafe_update(bus)` — runs the FAILSAFE chain, publishes
 *      `DigitalComBusID::FAILSAFE` on the bus.
 *   4. Chantier 12.5 — `remote_link_fallback_update(bus)` (only if
 *      `HAS_REMOTE_LINK_LOST_FALLBACK` is defined): observes
 *      `REMOTE_LINK_LOST` and forces RUNLEVEL = IDLE on the rising edge.
 *
 *   Returns `void`.  Consumers read `comBus.digitalBus[DigitalComBusID::FAILSAFE]`
 *   for the aggregated Failsafe state.
 */
void sys_manager_update(ComBus& bus);


/**
 * @brief Pre-clear only — NO-OP since Chantier 12.6.
 *
 * @details Kept for source-level compatibility with out-of-tree callers
 *   (sound node, etc.).  The historical `bus.isDrived = false;` is gone —
 *   link health is now monitored per-link by the contributors (each
 *   backend re-arms its own *_LINK_LOST channel every cycle).
 */
void sys_manager_reset(ComBus& bus);

// EOF sys_manager.h