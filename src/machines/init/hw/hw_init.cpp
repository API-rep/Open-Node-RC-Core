/*****************************************************************************
 * @file hw_init.cpp
 * @brief Implementation of hardware initialization routines
 *****************************************************************************/

#include "hw_init.h"
#include <core/config/machines/combus_types.h>
#include <core/system/debug/logging/debug.h>
#include <core/system/vbat/vbat.h>

// =============================================================================
// 1. HARDWARE CONFIG CHECK
// =============================================================================

/**
 * @brief Verify hardware configuration coherence.
 *
 * @details Delegates to sub-module checks for drivers.
 *   Servo and signal devices are checked inside servoInit() / sigDevInit().
 *   Halts the system when a critical error is detected.
 */
static void checkHwConfig() {
  hw_log_info("[HW] Configuration files check...\n");
  bool hasError = false;

  hasError |= checkDrvHwConfig(machine);

  if (hasError) {
    hw_log_err("  [HW] FATAL: Config check failed — system halted\n");
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

	// --- 1. Driver init ---
  hw_log_info("  [HW] Driver init\n");
  dcDriverInit(machine);
  hw_log_info("  [HW] Driver init complete\n\n");

	// --- 2. Servo init ---
  hw_log_info("  [HW] Servo init\n");
  servoInit(machine);
  hw_log_info("  [HW] Servo init complete\n\n");

	// --- 3. Signal device init ---
  hw_log_info("  [HW] Signal device init\n");
  sigDevInit(machine);
  hw_log_info("  [HW] Signal device init complete\n\n");

	// --- 4. Battery init ---
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
