/******************************************************************************
 * @file init.cpp
 * @brief Implementation of the main initialization sequence
 *
 * @details Calls sub-init modules in order: sys_init → hw_init → input_init.
 *   Boot-safe runlevel is applied after all hardware is initialized.
 *****************************************************************************/

#include "init.h"
#include "../utils/drv_control.h"


// =============================================================================
// 1. MAIN INIT SEQUENCE
// =============================================================================

/**
 * @brief Full initialization sequence — single entry point from setup().
 *
 * @details Runs sys_init, hw_init and input_init in order, then applies
 *   boot-safe runlevel to ensure all hardware starts in a known safe state.
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

	// --- 4. Boot-safe runlevel ---
  sys_log_info("[SYSTEM] Applying boot-safe runlevel...\n");
  comBus.runLevel = DEF_RUNLEVEL;
  stopAllDcDrivers(machine);
  sleepAllDcDrivers(machine);
  disableAllDcDrivers(machine);

#ifdef VBAT_SENSING
  pinMode(VBAT_SENSE_PIN, INPUT);
#endif

  sys_log_info("\n========================================\n");
  sys_log_info("  INIT COMPLETE — machine=%s\n", machine.infoName);
  sys_log_info("========================================\n\n");
}

// EOF init.cpp
