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

// EOF hw_init_srv.cpp
