/*****************************************************************************
 * @file hw_init.cpp
 * @brief Implementation of hardware initialization routines
 *****************************************************************************/

#include "hw_init.h"
#include <core/config/combus/combus.h>
#include <core/utils/debug/debug.h>
#include <core/utils/vbat/vbat_sense.h>

// =============================================================================
// 1. HARDWARE CONFIG CHECK
// =============================================================================

/**
 * @brief Verify hardware configuration coherence.
 *
 * @details Delegates to sub-module checks for drivers and servos.
 *   Halts the system when a critical error is detected.
 */
static void checkHwConfig() {
  hw_log_info("  [HW] Configuration files check...\n");
  bool hasError = false;

  hasError |= checkDrvHwConfig(machine);
  hasError |= checkSrvHwConfig(machine);

  if (hasError) {
    hw_log_err("  [HW] FATAL: Config check failed \u2014 system halted\n");
    while(1);
  }

  hw_log_info("  -> Configuration files check successfully\n");
  hw_log_info("\n");
}


// =============================================================================
// 2. MAIN HARDWARE INIT ROUTINE
// =============================================================================

/**
 * @brief Main hardware initialization \u2014 config check then hardware setup.
 */
void hw_init() {
  hw_log_info("[HW] Hardware init start\n");

	// --- 1. Config check ---
  checkHwConfig();

	// --- 2. Hardware setup ---
  hw_log_info("  [HW] Hardware setup...\n");
  applyParentConfig(machine);
  allocateDrivers(machine.dcDevCount);
  dcDriverInit(machine);
  servoInit(machine);

	// --- 3. Battery sensing init ---
  hw_log_info("  [HW][BAT] Battery sensing init: %d channel(s) configured\n", vBatSense.count);
  vbat_init(vBatSense);

	// --- 4. Battery sensing init summary ---
  uint8_t batActive = 0;
  for (uint8_t i = 0; i < vBatSense.count; i++) {
    if (!vBatSense.state[i].disabled) batActive++;
  }
  hw_log_info("  [HW][BAT] %d/%d channel(s) active\n", batActive, vBatSense.count);

  hw_log_info("  [HW] Hardware setup complete\n");

  hw_log_info("[HW] Hardware init complete\n");
}

// EOF hw_init.cpp
