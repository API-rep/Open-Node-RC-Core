/*****************************************************************************
 * @file hw_init_srv.cpp
 * @brief Implementation of servo initialization routines
 *****************************************************************************/

#include <core/config/machines/combus_types.h>
#include <struct/struct.h>
#include <core/system/debug/debug.h>
#include "hw_init_srv.h"
#include "../sys/sys_init.h"

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// Global pointer to Servo object array ---
ServoCore* srvDevObj = nullptr;

/**
 * @brief Initialize and allocate Servo objects in RAM.
 */

void allocateServos(int8_t count) {
  if (count <= 0) return;

    // Allocate the servo object array.
  srvDevObj = new ServoCore[count];
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
  if (config.srvDev == nullptr || config.srvDevCount == 0) {
    hw_log_info("    [SRV] No servo devices to initialize.\n");
    return;
  }

    // 2. Always run config coherence checks, even when called standalone.
  if (checkSrvHwConfig(config)) {
    hw_log_err("    [SRV] FATAL: Invalid servo configuration — init aborted\n");
    return;
  }

    // 3. Allocate servo objects.
  allocateServos(config.srvDevCount);

    // 4. Initialize each servo from config.
  for (int i = 0; i < config.srvDevCount; i++) {
    const SrvDevice* currentDev = &config.srvDev[i];

      // 4.1 Skip if device has no servo port mapping.
    if (currentDev->srvPort == nullptr || !currentDev->srvPort->pwmPin) {
      hw_log_warn("      [SRV] WARNING: SRV_%d has no servo port mapping\n", currentDev->ID);
      continue;
    }

      // 4.2 Claim pin ownership before touching ServoCore.
    const uint8_t pwmPin = *currentDev->srvPort->pwmPin;
    const char* pinLabel = (currentDev->infoName != nullptr) ? currentDev->infoName : "SRV";

    if (!pin_claim(pinReg, pwmPin, PinOwner::ServoOut, pinLabel, false)) {
      hw_log_warn("      [SRV] WARNING: SRV_%d skipped (GPIO%d already claimed)\n", currentDev->ID, pwmPin);
      continue;
    }

      // 4.3 Apply device descriptor then attach to the PWM pin.
    if (currentDev->pwmFreq) {
      srvDevObj[i].setPwmFreq(*currentDev->pwmFreq);
    }

    if (!srvDevObj[i].setTickDuration(currentDev->minUsTick, currentDev->maxUsTick)) {
      hw_log_err("      [SRV] ERROR: SRV_%d invariant broken on tick duration (min=%u max=%u)\n",
                 currentDev->ID,
                 currentDev->minUsTick,
                 currentDev->maxUsTick);
      continue;
    }

    if (!srvDevObj[i].setHwAngles(currentDev->hwAngle.minHwAngle, currentDev->hwAngle.maxHwAngle)) {
      hw_log_err("      [SRV] ERROR: SRV_%d invariant broken on hwAngle range (min=%.1f max=%.1f)\n",
                 currentDev->ID,
                 currentDev->hwAngle.minHwAngle,
                 currentDev->hwAngle.maxHwAngle);
      continue;
    }

    if (!srvDevObj[i].begin(pwmPin)) {
      hw_log_err("      [SRV] ERROR: SRV_%d begin() failed on GPIO%d\n",
                 currentDev->ID,
                 pwmPin);
      continue;
    }

    int8_t chId = currentDev->comChannel.has_value() ? static_cast<int8_t>(currentDev->comChannel.value()) : -1;
    hw_log_info("      > SRV_%d attached to pin %d on com channel %d\n", currentDev->ID, pwmPin, chId);
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

      // Validate hwAngle range if configured.
    const SrvHwAngle& ang = config.srvDev[i].hwAngle;
    if (ang.totalRange() <= 0.0f) {
      hw_log_err("\n      [SRV] CONFIG ERROR: SRV_%d hwAngle invalid range (min=%.1f max=%.1f)\n",
                 config.srvDev[i].ID, ang.minHwAngle, ang.maxHwAngle);
      hasError = true;
    }

      // Validate PWM tick range.
    const uint16_t minUs = config.srvDev[i].minUsTick;
    const uint16_t maxUs = config.srvDev[i].maxUsTick;
    if (minUs == 0 || maxUs <= minUs) {
      hw_log_err("\n      [SRV] CONFIG ERROR: SRV_%d usTick invalid (min=%u max=%u)\n",
                 config.srvDev[i].ID, minUs, maxUs);
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
