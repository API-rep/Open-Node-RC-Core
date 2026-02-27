/*****************************************************************************
 * @file hw_init.cpp
 * @brief Implementation of hardware initialization routines
 *****************************************************************************/

#include "hw_init.h"
#include <core/config/combus/combus.h>
#include <core/utils/debug/debug.h>

// =============================================================================
// 1. MAIN HARDWARE CONFIGURATION ROUTINE
// =============================================================================

/**
 * @brief Main hardware configuration routine
 */
void machine_hardware_setup() {

  hw_log_info("\n========================================\n");
  hw_log_info("      INITIALIZATION SEQUENCE\n");
  hw_log_info("========================================\n");
  hw_log_info("[HW] Starting hardware setup...\n");

	// --- 1. Start initialization pipeline ---

	// --- 2. Prepare data (Inheritance) ---
  applyParentConfig(machine);

	// --- 3. Allocate motor objects in RAM ---
  allocateDrivers(machine.dcDevCount);

	// --- 4. Initialize driver hardware ---
  dcDriverInit(machine);

	// --- 5. Initialize servo hardware ---
  servoInit(machine);

	// --- 6. Optional verbose debug output ---
  LOG_HW_CONFIG();

  hw_log_info("[HW] Hardware setup complete.\n");
  hw_log_info("========================================\n\n");
}

// =============================================================================
// 2. HARDWARE SANITY CHECKS
// =============================================================================

/**
 * @brief Verify hardware configuration coherence
 */
void checkHwConfig() {
  bool configError = false;

	// --- Validate DC driver indexing ---
  for (int i = 0; i < machine.dcDevCount; i++) {
    if (machine.dcDev[i].ID != i) {
      hw_log_err("!!! CONFIG ERROR : Driver index [%d] mismatch with driverID (%d)\n",
                 i, machine.dcDev[i].ID);
      configError = true;
    }
  }

  if (configError) {
    hw_log_err("SYSTEM HALTED : Critical config error for %s\n", machine.infoName);
    while(1);
  }
}

// EOF hw_init.cpp
