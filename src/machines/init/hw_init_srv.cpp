/*****************************************************************************
 * @file hw_init_srv.cpp
 * @brief Implementation of servo initialization routines
 *****************************************************************************/

#include <core/config/combus/combus_types.h>
#include <struct/struct.h>
#include <core/system/debug/debug.h>
#include "hw_init_srv.h"

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to Servo object array ---
ESP32_PWM_Servo* srvDevObj = nullptr;

/**
 * @brief Initialize and allocate Servo objects in RAM.
 */
void allocateServos(int8_t count) {
  if (count <= 0) return;

  // Heap-allocate the servo object array.
  srvDevObj = new ESP32_PWM_Servo[count];
  hw_log_info("    [SRV] Allocated memory for %d servo devices\n", count);
}

// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * @brief Initialize servo devices from machine configuration.
 */
void servoInit(const Machine &config) {
  hw_log_info("    [SRV] Initializing servo devices...\n");

  // 1. Early-out: no servo devices configured.
  if (config.srvDevCount == 0) {
    hw_log_info("    [SRV] No servo devices to initialize.\n");
    return;
  }

  // 2. Allocate servo objects.
  allocateServos(config.srvDevCount);

  // 3. Initialize each servo from config.
  for (int i = 0; i < config.srvDevCount; i++) {
    const SrvDevice* currentDev = &config.srvDev[i];

    // Skip if device has no servo port mapping.
    if (currentDev->srvPort == nullptr || !currentDev->srvPort->pwmPin) {
      hw_log_warn("      [SRV] WARNING: SRV_%d has no servo port mapping\n", currentDev->ID);
      continue;
    }

    int8_t chId = currentDev->comChannel.has_value() ? static_cast<int8_t>(currentDev->comChannel.value()) : -1;
    hw_log_info("      > SRV_%d attached to pin %d on com channel %d\n",
                currentDev->ID,
                *currentDev->srvPort->pwmPin,
                chId);

    // Placeholder: attach servo to PWM pin here.
  }

  // 4. Report completion.
  hw_log_info("    [SRV] Servo devices initialized\n");
}


// =============================================================================
// 3. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify servo configuration coherence.
 *
 * @details Checks that each servo array index matches its declared ID.
 *   Returns true when at least one error is detected — halting is the
 *   caller's responsibility.
 */
bool checkSrvHwConfig(const Machine &config) {
  hw_log_info("  [SRV] Servos config check...");
  bool hasError = false;

  // 1. Validate servo index vs declared ID.
  for (int i = 0; i < config.srvDevCount; i++) {
    if (config.srvDev[i].ID != i) {
      hw_log_err("\n      [SRV] CONFIG ERROR: Servo index [%d] mismatch with srvID (%d)\n",
                 i, config.srvDev[i].ID);
      hasError = true;
    }
  }

  // 2. Report overall result.
  if (!hasError) {
    hw_log_info(" OK\n");
  }

  return hasError;
}

// EOF hw_init_srv.cpp
