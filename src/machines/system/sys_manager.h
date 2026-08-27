/******************************************************************************
 * @file sys_manager.h
 * @brief System-level tick orchestrator — single entry point per loop cycle.
 *
 * @details Centralises all recurring system services that must run once per
 *   loop, in a deterministic order, before application logic:
 *
 *   1. Open-drain pre-clear of `bus.isDrived` (written false at the top of
 *      every cycle; re-asserted true by any active physical input source).
 *   2. Input acquisition: core `input_refresh()` then machine
 *      `input_update(bus)` — re-asserts `bus.isDrived` when a physical
 *      controller is connected.
 *   3. Battery update via `vbat_update()` — ADC sensing, sliding average,
 *      low-bat detection, and re-arm of `DigitalComBusID::FAILSAFE_VBAT`.
 *   4. Failsafe update via `failsafe_update(bus)` — runs the FAILSAFE
 *      processor chain (reset + contributors), publishes the aggregated
 *      `DigitalComBusID::FAILSAFE` channel.
 *
 *   **Open-drain invariant:**
 *   - `bus.isDrived = false` is written ONLY by `sys_manager_reset()`.
 *   - `bus.isDrived = true`  is written ONLY by active physical input sources
 *     (PS4 controller in `input_update`, UART frame in `combus_frame_apply`).
 *   - No other module writes this flag in either direction.
 *
 *   **Why this file lives under machines/ and not core/:**
 *   Step 2 calls this machine's own `input_update(bus)` (the input device ->
 *   ComBus mapping — see machines/system/input/input_update.h), which is
 *   machine-specific by nature. Keeping sys_manager here avoids a core file
 *   depending on a machine path. The trade-off: each machine owns its own
 *   copy of this orchestrator. If that duplication becomes painful across
 *   several machines, consider factoring the generic part (pre-clear / vbat /
 *   failsafe) back into core behind a registered input hook — ask if/when
 *   that's worth revisiting.
 *
 *   Machine node: call `sys_manager_update()` once as the first statement
 *   in `loop()`.
 *
 *   Sound node: call `sys_manager_reset()` before the UART frame interpreter;
 *   `combus_frame_apply()` re-asserts `isDrived` when a valid frame is applied.
 *
 *   **Failsafe deprecation note (WIP §12.4):**
 *   `SysResult::failsafeActive` is the legacy open-drain flag (true when no
 *   physical input source refreshed the bus this cycle). The new Failsafe
 *   module (`src/core/system/failsafe/`, see
 *   `doc/WIP - Failsafe module design.md`) publishes the aggregated
 *   `DigitalComBusID::FAILSAFE` ComBus channel via the processor chain.
 *   Consumers should migrate from `SysResult::failsafeActive` to
 *   `comBus.digitalBus[DigitalComBusID::FAILSAFE]`. Both coexist until the
 *   reaction chain migration is complete (WIP §12.9 / §12.12).
 *
 *   Migration rules in effect until then:
 *     - producers must keep writing `failsafeActive` unchanged;
 *     - consumers must keep reading `failsafeActive` unchanged;
 *     - the legacy reaction path must remain operational.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>


// =============================================================================
// 1. RESULT TYPE
// =============================================================================

/**
 * @brief Return value of sys_manager_update() — system tick summary.
 *
 * @details `failsafeActive` is the legacy open-drain flag (true when no
 *   physical input source refreshed the bus this cycle). Kept temporarily
 *   for backward compatibility with the legacy reaction path. See the
 *   file-level deprecation note.
 */
struct SysResult {
    bool failsafeActive;  ///< true = no active input source detected this cycle.
                          ///<  @deprecated Kept temporarily for backward compatibility.
                          ///<  Migrate to comBus.digitalBus[DigitalComBusID::FAILSAFE]
                          ///<  once the reaction chain (WIP §12.9 / §12.12) is validated.
    bool vbatChanged;     ///< true = at least one vbat channel changed state
                          ///<  @deprecated Hard-wired to false since A16.2 — the
                          ///<  Failsafe chain re-evaluates every cycle.
};


// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Full system tick — call ONCE per loop, BEFORE application logic.
 *
 * @details Executes in order:
 *   1. Pre-clear `bus.isDrived` (open-drain reset).
 *   2. `input_refresh()` (core) — physical acquisition.
 *      `input_update(bus)` (machine) — input -> ComBus mapping.
 *   3. `vbat_update()` — battery sensing + re-arm of `FAILSAFE_VBAT`.
 *   4. `failsafe_update(bus)` — runs the FAILSAFE chain, publishes
 *      `DigitalComBusID::FAILSAFE` on the bus.
 *   5. Returns `SysResult { failsafeActive, vbatChanged }`.
 *
 * @return SysResult with `failsafeActive` and `vbatChanged` flags.
 *         Consumers should also read `comBus.digitalBus[DigitalComBusID::FAILSAFE]`
 *         for the aggregated Failsafe state.
 */
SysResult sys_manager_update(ComBus& bus);


/**
 * @brief Pre-clear only — for nodes where input sources run outside sys_manager.
 *
 * @details Writes `bus.isDrived = false` and nothing else.
 *   Use on the sound node before the UART frame interpreter;
 *   `combus_frame_apply()` re-asserts `isDrived` automatically on a valid frame.
 */
void sys_manager_reset(ComBus& bus);

// EOF sys_manager.h
