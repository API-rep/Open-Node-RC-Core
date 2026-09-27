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
  if constexpr (PauseAfterInit) {
      // KEY channel index in the digital bus (TRIANGLE on PS4 in dumper-truck layout)
    uint8_t keyCh = static_cast<uint8_t>(DigitalComBusID::KEY_BTN);
    sys_log_info("[SYSTEM] ** Paused ** — press ENTER or IGNITION KEY to continue...\n");

      // Edge detection: the KEY button semantics is an EVENT (rising edge),
      // not a LEVEL.  A brief PS4_BT press (< 50 ms) is enough to release the
      // pause — we no longer require the player to hold TRIANGLE while the
      // loop polls.  The previous level-based check forced operators to use
      // Serial ENTER because the BT poll latency could miss short presses.
    bool prevKey = comBus.digitalBus[keyCh].value;
    while (true) {
        // Keep the combus alive during the wait (BT connection, input watchdog)
      input_update(comBus);

        // Exit via serial: ENTER only (CR or LF) — discard stray bytes (ROM noise, BT traces)
      while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') goto pause_exit;
      }
        // Exit via remote KEY channel — RISING EDGE (false → true).
        // Chantier 12.6: `bus.isNotDrived` removed — use REMOTE_LINK_LOST
        // (LOCAL aggregator) when HAS_REMOTE_LINK_LOST_FALLBACK is defined,
        // otherwise fall back to the lastFrameMs proxy.  The link check only
        // applies to the rising-edge condition; a disconnected controller
        // cannot legitimately trigger KEY.
      bool currKey = comBus.digitalBus[keyCh].value;
#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
      if (!comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::REMOTE_LINK_LOST)].value
          && currKey && !prevKey) break;
#else
      if ((millis() - comBus.lastFrameMs) < 2000u
          && currKey && !prevKey) break;
#endif
      prevKey = currKey;

      vTaskDelay(10);  // yield — avoid starving the scheduler
    }
    pause_exit:

    sys_log_info("[SYSTEM] Pause released — entering main loop.\n\n");
  }

	  // --- 8. Start dashboard FreeRTOS task (after pause, on Core 0) ---
	  // Called here — and not inside dashboard_machine_setup() — so the task
	  // does not activate during the PAUSE_LOG_AFTER_INIT wait.
  dashboard_start_task();
}

// EOF init.cpp
