/*****************************************************************************
 * @file hw_init_srv.cpp
 * @brief Implementation of servo initialization routines
 *****************************************************************************/

#include <core/config/combus/combus.h>
#include <struct/struct.h>
#include <core/utils/debug/debug.h>
#include "hw_init_srv.h"

// =============================================================================
// 1. SERVO INITIALIZATION LOGIC
// =============================================================================

ESP32_PWM_Servo* srvDevObj = nullptr;

/**
 * @brief Initialize and allocate servo objects in RAM
 */

void allocateServos(int8_t count) {
  if (count <= 0) return;

  srvDevObj = new ESP32_PWM_Servo[count];
  hw_log_info("  [SRV] Allocated memory for count=%d servo devices\n", count);
}

/**
 * @brief Main servo initialization function
 */
void servoInit(const Machine &config) {
  hw_log_info("  [SRV] Initializing servo devices...\n");
  
	// --- Check if servos are defined in machine configuration ---
  if (config.srvDevCount > 0) {
    allocateServos(config.srvDevCount);

    for (int i = 0; i < config.srvDevCount; i++) {
      const SrvDevice* currentDev = &config.srvDev[i];

      if (currentDev->srvPort == nullptr || !currentDev->srvPort->pwmPin) {
        hw_log_warn("    - WARNING: SRV_%d has no servo port mapping\n", currentDev->ID);
        continue;
      }

      int8_t chId = currentDev->comChannel.has_value() ? static_cast<int8_t>(currentDev->comChannel.value()) : -1;
      hw_log_info("    > SRV_%d attached to pin %d on com channel %d\n",
                  currentDev->ID,
                  *currentDev->srvPort->pwmPin,
                  chId);
        
		// --- Initializing servo device objects ---
		// Placeholder for your servo init logic
    }

    hw_log_info("  [SRV] Servo devices initialized\n");
  }
  else {
    hw_log_info("  [SRV] No servo devices to initialize.\n");
  }

  hw_log_info("\n");
}


// =============================================================================
// 2. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify servo configuration coherence.
 *
 * @details Checks that each servo array index matches its declared ID.
 *   Returns true when at least one error is detected \u2014 halting is the
 *   caller's responsibility.
 */
bool checkSrvHwConfig(const Machine &config) {
  hw_log_info("    [HW][SRV] Config check...\n");
  bool hasError = false;

	// --- Validate servo index vs declared ID ---
  for (int i = 0; i < config.srvDevCount; i++) {
    if (config.srvDev[i].ID != i) {
      hw_log_err("    [HW][SRV] CONFIG ERROR: Servo index [%d] mismatch with srvID (%d)\n",
                 i, config.srvDev[i].ID);
      hasError = true;
    }
  }

  if (!hasError) {
    hw_log_info("    [HW][SRV] Config check passed\n");
  }

  return hasError;
}

// EOF hw_init_srv.cpp
