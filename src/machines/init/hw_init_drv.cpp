/*****************************************************************************
 * @file hw_init_drv.cpp
 * @brief Implementation of DC drivers allocation and hardware setup
 *****************************************************************************/

#include <core/config/combus/combus.h>
#include <struct/struct.h>

#include "hw_init_drv.h"

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to DC motor object array ---
ESP32_PWM_Motor* dcDevObj = nullptr;

/**
 * @brief Initialize and allocate DC driver objects in RAM
 */
void allocateDrivers(int8_t count) {
  if (count <= 0) return;

	// --- Dynamic allocation of the motor controller array ---
  dcDevObj = new ESP32_PWM_Motor[count];
  Serial.printf("[SYSTEM] Allocated memory for %d DC drivers\n", count);
}

// =============================================================================
// 2. CONFIGURATION INHERITANCE
// =============================================================================

/**
 * @brief Apply parent's configuration to child drivers (Cloning logic)
 */
void applyParentConfig(const Machine &config) {
  for (int i = 0; i < config.dcDevCount; i++) {
    DcDevice* child = &config.dcDev[i];

    if (child->parentID) {
      bool parentIsFound = false;

	      // Search for matching parent ID in the device list
      for (int j = 0; j < config.dcDevCount; j++) {
        const DcDevice* parent = &config.dcDev[j];

          // --- Copy missing fields from parent to child ---
        if (parent->ID == *child->parentID) {
          if (child->DevType == DcDevType::UNDEFINED)    child->DevType      = parent->DevType;
          if (child->usage == DevUsage::UNDEFINED)       child->usage        = parent->usage;
          if (child->mode == DcDrvMode::UNDEFINED)       child->mode         = parent->mode;
          if (!child->comChannel)                        child->comChannel   = parent->comChannel;
          if (!child->pwmFreq)                           child->pwmFreq      = parent->pwmFreq;
          if (!child->maxFwSpeed)                        child->maxFwSpeed   = parent->maxFwSpeed;
          if (!child->maxBackSpeed)                      child->maxBackSpeed = parent->maxBackSpeed;

          parentIsFound = true;
          break; 
        }
      }

      if (!parentIsFound) {
        Serial.printf(PSTR("FATAL: Parent ID %d not found for driver %s\n"), *child->parentID, child->infoName);
        while(1);
      }
    }
  }
}

// =============================================================================
// 3. HARDWARE INITIALIZATION
// =============================================================================

/**
 * @brief Initialize DC drivers hardware from machine configuration
 */
void dcDriverInit(const Machine &config) {
	  // --- 1. Safety check: ensure drivers are configured ---
  if (config.dcDev == nullptr || config.dcDevCount <= 0) {
    Serial.println(F("[DRV] No DC devices to initialize."));
    return;
  }

	  // --- 2.Initialize each DC driver from config ---
  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* currentDev = &config.dcDev[i];

	    // Skip if device has no DC driver port mapping
    if (currentDev->drvPort == nullptr || !currentDev->drvPort->pwmPin) {
      Serial.printf("[DRV] FATAL: Device '%s' has no DC driver port mapping!\n", currentDev->infoName);
      continue;
    }

    uint8_t pwmPin = *currentDev->drvPort->pwmPin;

	    // CLONE MODE: Synchronize PWM timer with parent
    if (currentDev->parentID) {
      uint8_t pID = *currentDev->parentID;
      if (pID < config.dcDevCount) {
        dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
        dcDevObj[i].attach(pwmPin);
        Serial.printf("[DRV] ID:%d (Clone) attached to Pin:%d (Sync with Parent:%d)\n", currentDev->ID, pwmPin, pID);
      }
    }

	    // MASTER MODE: Independent setup
    else {
      if (currentDev->pwmFreq) {
        dcDevObj[i].attach(pwmPin, -1, *currentDev->pwmFreq);
        Serial.printf("[DRV] ID:%d (Master) attached to Pin:%d at %u Hz\n", currentDev->ID, pwmPin, *currentDev->pwmFreq);
      } 
      
      else {
        dcDevObj[i].attach(pwmPin);
        Serial.printf("[DRV] ID:%d (Master) attached to Pin:%d (Default Freq)\n", currentDev->ID, pwmPin);
      }
    }

	    // --- 3. HARDWARE SECURITY (SLEEP & ENABLE) ---
    if (currentDev->drvPort->slpPin && currentDev->drvPort->driverModel) {
        // Bridge between custom InputPinMode enum and Library defines
      uint8_t libSleepMode;

        // TODO: Refactor library to use InputPinMode enum directly
      if (currentDev->drvPort->driverModel->sleepPinMode == InputPinMode::ACTIVE_LOW) {
        libSleepMode = ESP32_PWM_MOTOR_ACTIVE_LOW; // Library ACTIVE_LOW
      } 
      else {
        libSleepMode = ESP32_PWM_MOTOR_ACTIVE_HIGH; // Library ACTIVE_HIGH
      }

        // Configure pin and force SLEEP state immediately for boot safety
        // We use * to get the value from the optional slpPin
      dcDevObj[i].setSleepPin(*currentDev->drvPort->slpPin, libSleepMode);
      dcDevObj[i].sleep(); 

      Serial.printf("[DRV] ID:%d | Sleep Pin:%d | Mode:%d | State: LOCKED\n", 
                    currentDev->ID, *currentDev->drvPort->slpPin, libSleepMode);
    }

      // Initial STOP command to ensure driver is idle
    dcDevObj[i].stop();
  }
}

// EOF hw_init_drv.cpp
