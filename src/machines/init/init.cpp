/******************************************************************************
 * @file init.cpp
 * @brief Implementation of the main initialization sequence
 *
 * @details Calls sub-init modules in order: sys_init → hw_init → input_init.
 *   hw_init() internally opens the communication transport first (pin claim)
 *   then initialises drivers, servos, signal devices and battery sensing.
 *   Boot-safe runlevel is applied after all hardware is initialized.
 *****************************************************************************/

#include "init.h"
#include "../system/drv_control.h"
#include <machines/system/input/input_update.h>
#include <core/system/combus/combus_access.h>
#include <machines/system/debug/dashboard_machine.h>
#include <core/system/debug/dashboard/dashboard.h>


// =============================================================================
// 1. MAIN INIT SEQUENCE
// =============================================================================

/**
 * @brief Full initialization sequence — single entry point from setup().
 *
 * @details Runs sys_init, hw_init and input_init in order, then applies
 *   boot-safe runlevel to ensure all hardware starts in a known safe state.
 *   dashboard_setup() is called last so the dashboard starts with a fully
 *   initialized bus and machine config.
 *
 * @note Two independent concerns live in this init sequence:
 *   1. **Default runlevel** — applied unconditionally at step 5 below
 *      (DEF_RUNLEVEL = RunLevel::IDLE).  This is a general-code decision
 *      independent of any build flag.
 *   2. **Dashboard pause** — when -D PAUSE_LOG_AFTER_INIT is set, the main
 *      loop is held until the operator presses ENTER on the serial monitor.
 *      This is purely a "give the operator time to read the boot log"
 *      gate.  Only CR/LF is accepted as a serial exit trigger — stray bytes
 *      (ESP32 ROM boot noise, BT stack traces) are silently discarded.
 *      The pause block is fully stripped from the binary when the flag is
 *      absent.  No remote control input is consulted here — KEY_BTN is
 *      reserved for the runlevel FSM, which activates once loop() starts.
 */
void machine_init() {

  sys_log_info("\n========================================\n");
  sys_log_info("       MACHINE INIT SEQUENCE\n");
  sys_log_info("========================================\n");

	  // --- 1. System init ---
  sys_init();

	  // --- 2. Hardware init ---
  hw_init();

	  // --- 3. Input init ---
  input_init();

	  // --- 4. Output init ---
  output_init();

	  // --- 5. Boot-safe runlevel ---
  sys_log_info("[SYSTEM] Applying boot-safe runlevel...\n");
  // RL3: runLevel is now a plain analog channel — write via generic accessor.
  combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)DEF_RUNLEVEL, ChanLayer::LOCAL);
  stopAllDcDrivers(machine);
  sleepAllDcDrivers(machine);
  disableAllDcDrivers(machine);

  sys_log_info("\n========================================\n");
  sys_log_info("  INIT COMPLETE — machine=%s\n", machine.infoName);
  sys_log_info("========================================\n\n");

	  // --- 6. Dashboard setup ---
  dashboard_machine_setup(&comBus, &machine, static_cast<uint8_t>(AnalogComBusID::CH_COUNT), static_cast<uint8_t>(DigitalComBusID::CH_COUNT));

  
	  // --- 7. Post-init pause (compiled in only when -D PAUSE_LOG_AFTER_INIT is set) ---
  //
  //   Scope:  ONLY a "give the operator time to read the boot log" gate.
  //   Default runlevel (DEF_RUNLEVEL = RunLevel::IDLE) is already applied
  //   above at step 5 — independent of this flag, runs at every boot.
  //
  //   Implementation: non-blocking.  The dashboard FreeRTOS task is started
  //   FIRST (step 7) so the suspend flag has an active owner.  Setting
  //   s_suspended = true (step 7b) before the task is created would have
  //   no effect — the task never observes it.  The application loop
  //   continues normally during the pause; only dashboard output is gated.
  //   No `goto`, no busy-loop on Serial.
  //
  //   Resuming is identical to the in-session suspend (Q key): any serial
  //   byte resumes the dashboard, no specific key required.
  if constexpr (PauseAfterInit) {
      // 7a. Spawn the FreeRTOS task on Core 0 (priority 1, 16 KB stack).
    dashboard_start_task();
      // 7b. Mark the dashboard as suspended and print the resume prompt.
      //     The task is now running and will sit in dashboard_update()'s
      //     s_suspended branch, consuming one character to resume.
    sys_log_info("[SYSTEM] Boot log complete — dashboard suspended, press any key to start.\n");
    dashboard_suspend_for_input("[DASH] Boot paused — press any key to start dashboard.");
  } else {
      // Release build: start the task immediately (no boot pause).
    dashboard_start_task();
  }
}

// EOF init.cpp
