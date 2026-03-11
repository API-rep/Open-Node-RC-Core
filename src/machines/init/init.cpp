/******************************************************************************
 * @file init.cpp
 * @brief Implementation of the main initialization sequence
 *
 * @details Calls sub-init modules in order: sys_init → hw_init → input_init.
 *   Boot-safe runlevel is applied after all hardware is initialized.
 *****************************************************************************/

#include "init.h"
#include "../system/drv_control.h"
#include <core/system/input/input_manager.h>
#include <machines/system/dashboard/dashboard_machine.h>


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
 * @note When -D PAUSE_LOG_AFTER_INIT is set, execution holds after INIT COMPLETE
 *   until the operator releases the pause — either by pressing ENTER on the
 *   serial monitor, or by holding the KEY button on the remote.
 *   Only CR/LF is accepted as a serial exit trigger — stray bytes (ESP32 ROM
 *   boot noise, BT stack traces) are silently discarded.
 *   The input module is polled actively during the wait so the combus stays live.
 *   The pause block is fully stripped from the binary when the flag is absent.
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
  comBus.runLevel = DEF_RUNLEVEL;
  stopAllDcDrivers(machine);
  sleepAllDcDrivers(machine);
  disableAllDcDrivers(machine);

  sys_log_info("\n========================================\n");
  sys_log_info("  INIT COMPLETE — machine=%s\n", machine.infoName);
  sys_log_info("========================================\n\n");

	  // --- 6. Dashboard setup ---
  dashboard_machine_setup(&comBus, &machine, static_cast<uint8_t>(AnalogComBusID::CH_COUNT), static_cast<uint8_t>(DigitalComBusID::CH_COUNT));

  
	  // --- 7. Post-init pause (compiled in only when -D PAUSE_LOG_AFTER_INIT is set) ---
  if constexpr (PauseAfterInit) {
      // KEY channel index in the digital bus (TRIANGLE on PS4 in dumper-truck layout)
    uint8_t keyCh = static_cast<uint8_t>(DigitalComBusID::KEY);
    sys_log_info("[SYSTEM] ** Paused ** — press ENTER or IGNITION KEY to continue...\n");

    while (true) {
        // Keep the combus alive during the wait (BT connection, input watchdog)
      input_update(comBus);

        // Exit via serial: ENTER only (CR or LF) — discard stray bytes (ROM noise, BT traces)
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') goto pause_exit;
      }
        // Exit via remote KEY channel
      if (comBus.digitalBus[keyCh].isDrived && comBus.digitalBus[keyCh].value) break;

      vTaskDelay(10);  // yield — avoid starving the scheduler
    }
    pause_exit:

    sys_log_info("[SYSTEM] Pause released — entering main loop.\n\n");
  }
}

// EOF init.cpp
