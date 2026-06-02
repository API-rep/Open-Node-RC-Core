/******************************************************************************
 * @file sys_manager.h
 * @brief System-level tick orchestrator — single entry point per loop cycle.
 *
 * @details Centralises all recurring system services that must run once per
 *   loop, in a deterministic order, before application logic:
 *
 *   1. Open-drain pre-clear of `bus.isDrived` (written false at the top of
 *      every cycle; re-asserted true by any active physical input source).
 *   2. Input acquisition via `input_update()` — re-asserts `bus.isDrived`
 *      when a physical controller is connected.
 *   3. Battery sensing tick via `vbat_sense_tick()`.
 *   4. Failsafe evaluation: `failsafeActive = !bus.isDrived`.
 *
 *   **Open-drain invariant:**
 *   - `bus.isDrived = false` is written ONLY by `sys_manager_reset()`.
 *   - `bus.isDrived = true`  is written ONLY by active physical input sources
 *     (PS4 controller in `input_update`, UART frame in `combus_frame_apply`).
 *   - No other module writes this flag in either direction.
 *
 *   Machine node: call `sys_manager_update()` once as the first statement
 *   in `loop()`.
 *
 *   Sound node: call `sys_manager_reset()` before the UART frame interpreter;
 *   `combus_frame_apply()` re-asserts `isDrived` when a valid frame is applied.
 *****************************************************************************/
#pragma once

#include <struct/combus_struct.h>


// =============================================================================
// 1. RESULT TYPE
// =============================================================================

/**
 * @brief Return value of sys_manager_update() — system tick summary.
 */
struct SysResult {
    bool failsafeActive;  ///< true = no active input source detected this cycle
    bool vbatChanged;     ///< true = at least one vbat channel changed state
};


// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Full system tick — call ONCE per loop, BEFORE application logic.
 *
 * @details Executes in order:
 *   1. Pre-clear `bus.isDrived` (open-drain reset).
 *   2. `input_update(bus)` — acquires all physical input sources.
 *   3. `vbat_sense_tick()` — battery ADC read and low-bat detection.
 *   4. Evaluates `failsafeActive = !bus.isDrived`.
 *
 * @return SysResult with `failsafeActive` and `vbatChanged` flags.
 *         Caller is responsible for battery-channel writes
 *         (`DigitalComBusID::BATTERY_LOW`, `combus_set_battlow`) and the
 *         battery-triggered runlevel transition using `vbatChanged`.
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
