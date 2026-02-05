/*****************************************************************************
 * @file drv_control.cpp
 * @brief Implementation of DC drivers batch control functions
 *****************************************************************************/

#include "drv_control.h"
#include <init/hw_init_drv.h> // Required for dcDevObj access
#include <Arduino.h>

// =============================================================================
// 1. DRIVER STATE LOGIC
// =============================================================================

/**
 * @brief Put all DC drivers into sleep mode
 */
void sleepAllDcDrivers(const Machine &config) {
  if (config.dcDev == nullptr || config.dcDevCount <= 0) return;

  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* d = &config.dcDev[i];

	    // --- Accessing hardware Sleep pin via library ---
    if (d->drvPort->slpPin) {
      dcDevObj[i].sleep();
      Serial.printf("[DRV] ID:%d put to SLEEP\n", d->ID);
    }
  }
}


/**
 * @brief Wake up all DC drivers
 */
void wakeupAllDcDrivers(const Machine &config) {
  if (config.dcDev == nullptr || config.dcDevCount <= 0) return;

  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* d = &config.dcDev[i];

	    // --- Accessing hardware Sleep pin via library ---
    if (d->drvPort->slpPin) {
      dcDevObj[i].wakeup();
      Serial.printf("[DRV] ID:%d WOKEN UP\n", d->ID);
    }
  }
}


/**
 * @brief Enable all DC drivers output bridges
 * TODO: Implement motor.enable() function in ESP32_PWM_DRIVER lib with active high/LOW strate selection
 */
void enableAllDcDrivers(const Machine &config) {
  if (config.dcDev == nullptr || config.dcDevCount <= 0) return;

  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* d = &config.dcDev[i];

	    // --- Direct GPIO control for Enable pin ---
    if (d->drvPort->enPin) {
      digitalWrite(*d->drvPort->enPin, HIGH);
      Serial.printf("[DRV] ID:%d ENABLED\n", d->ID);
    }
  }
}


/**
 * @brief Disable all DC drivers output bridges
 */
void disableAllDcDrivers(const Machine &config) {
  if (config.dcDev == nullptr || config.dcDevCount <= 0) return;

  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* d = &config.dcDev[i];

	// --- 1. Cut off physical bridge power via Enable pin ---
    if (d->drvPort->enPin) {
      digitalWrite(*d->drvPort->enPin, LOW);
      Serial.printf("[DRV] ID:%d DISABLED\n", d->ID);
    }
  }
}


/**
 * @brief Stop all DC drivers outputs
 */
void stopAllDcDrivers(const Machine &config) {
  if (config.dcDev == nullptr || config.dcDevCount <= 0) return;

  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* d = &config.dcDev[i];

      // --- Sets motor speed to 0 via library and handles bridge brake/coast ---
    dcDevObj[i].stop();
    Serial.printf("[DRV] ID:%d STOPPED\n", i);
  }
}

// EOF drv_control.cpp
