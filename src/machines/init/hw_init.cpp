/*****************************************************************************
 * @file hw_init.cpp
 * @brief Implementation of hardware initialization routines
 *****************************************************************************/

#include "hw_init.h"
#include <core/config/combus/combus.h>

// =============================================================================
// 1. MAIN HARDWARE CONFIGURATION ROUTINE
// =============================================================================

/**
 * @brief Main hardware configuration routine
 */
void machine_hardware_setup() {

	// --- 1. Start initialization pipeline ---

  Serial.println(F("[INIT] Starting Hardware Setup..."));

	// --- 2. Prepare data (Inheritance) ---
  applyParentConfig(machine);

	// --- 3. Allocate motor objects in RAM ---
  allocateDrivers(machine.dcDevCount);

	// --- 4. Initialize driver hardware ---
  dcDriverInit(machine);

	// --- 5. Optional verbose debug output ---
  LOG_HW_CONFIG();

  Serial.println(F("[INIT] Hardware Setup Complete."));
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
      Serial.printf("!!! CONFIG ERROR : Driver index [%d] mismatch with driverID (%d)\n", 
                    i, machine.dcDev[i].ID);
      configError = true;
    }
  }

  if (configError) {
    Serial.printf(PSTR("SYSTEM HALTED : Critical config error for %s\n"), machine.infoName);
    while(1);
  }
}

// EOF hw_init.cpp
