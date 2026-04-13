/*****************************************************************************
 * @file hw_init.cpp
 * @brief Implementation of hardware initialization routines
 *****************************************************************************/

#include "hw_init.h"
#include <core/config/machines/combus_types.h>
#include <core/system/debug/debug.h>
#include <core/system/vbat/vbat.h>

// =============================================================================
// 1. HARDWARE CONFIG CHECK
// =============================================================================

/**
 * @brief Verify hardware configuration coherence.
 *
 * @details Delegates to sub-module checks for drivers, servos, and signal devices.
 *   Halts the system when a critical error is detected.
 */
static void checkHwConfig() {
  hw_log_info("[HW] Configuration files check...\n");
  bool hasError = false;

  hasError |= checkDrvHwConfig(machine);
  hasError |= checkSrvHwConfig(machine);
  hasError |= checkSigHwConfig(machine);

  if (hasError) {
    hw_log_err("  [HW] FATAL: Config check failed \u2014 system halted\n");
    while(1);
  }

  hw_log_info("[HW] Configuration files check successfully\n");
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

	// --- 0. Communication transport init (pin claim — must be first) ---
  hw_init_com();

	// --- 1. Config check ---
  checkHwConfig();

	// --- 2. Driver init ---
  hw_log_info("  [HW] Driver init\n");
  applyParentConfig(machine);
  allocateDrivers(machine.dcDevCount);
  dcDriverInit(machine);
  hw_log_info("  [HW] Driver init complete\n\n");

	// --- 3. Servo init ---
  hw_log_info("  [HW] Servo init\n");
  servoInit(machine);
  hw_log_info("  [HW] Servo init complete\n\n");

	// --- 4. Signal device init ---
  hw_log_info("  [HW] Signal device init\n");
  sigDevInit(machine);
  hw_log_info("  [HW] Signal device init complete\n\n");

	// --- 5. Battery init ---
  hw_log_info("  [HW] Battery init\n");
  hw_log_info("    [BAT] Battery sensing init: %d channel(s) configured\n", vBatSense.count);
  vbat_init(&vBatSense);

  uint8_t batActive = 0;
  for (uint8_t i = 0; i < vBatSense.count; i++) {
    if (!vBatSense.state[i].disabled) batActive++;
  }
  hw_log_info("    [BAT] %d/%d channel(s) active\n", batActive, vBatSense.count);
  hw_log_info("  [HW] Battery init complete\n\n");

  hw_log_info("[HW] Hardware init complete\n\n");
}

// EOF hw_init.cpp
