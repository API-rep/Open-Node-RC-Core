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
  //   Scope of this block:  ONLY the wait before the dashboard task is started.
  //   The default runlevel is already applied above (step 5, boot-safe) and is
  //   independent of this flag — the system always starts at DEF_RUNLEVEL
  //   (RunLevel::IDLE) at the general code level.
  //
  //   This block exists purely to give the operator time to read the boot
  //   log on the serial monitor before the dashboard takes over the terminal.
  //   Only ENTER (CR or LF) on Serial releases the pause.  No remote control
  //   input is accepted here — KEY_BTN is reserved for the runlevel FSM,
  //   which becomes active once the main loop starts.
  if constexpr (PauseAfterInit) {
    sys_log_info("[SYSTEM] ** Paused ** — press ENTER to start the dashboard...\n");

    while (true) {
        // Exit via serial: ENTER only (CR or LF) — discard stray bytes (ROM noise, BT traces)
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') goto pause_exit;
      }

      vTaskDelay(10);  // yield — avoid starving the scheduler
    }
    pause_exit:

    sys_log_info("[SYSTEM] Pause released — starting dashboard.\n\n");
  }

	  // --- 8. Start dashboard FreeRTOS task (after pause, on Core 0) ---
	  // Called here — and not inside dashboard_machine_setup() — so the task
	  // does not activate during the PAUSE_LOG_AFTER_INIT wait.
  dashboard_start_task();
}

// EOF init.cpp
