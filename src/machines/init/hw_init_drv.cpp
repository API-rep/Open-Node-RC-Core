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

	// --- Search for matching parent ID in the device list ---
      for (int j = 0; j < config.dcDevCount; j++) {
        const DcDevice* parent = &config.dcDev[j];

        if (parent->ID == *child->parentID) {
		// --- Copy missing fields from parent to child ---
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
	  // --- Safety check: ensure drivers are configured ---
  if (config.dcDev == nullptr || config.dcDevCount <= 0) {
    Serial.println(F("[DRV] No DC devices to initialize."));
    return;
  }

	  // --- Initialize each DC driver from config ---
  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* currentDev = &config.dcDev[i];

	    // --- Skip if device has no hardware port mapping ---
    if (currentDev->drvPort == nullptr || !currentDev->drvPort->pwmPin) {
      Serial.printf("[DRV] FATAL: Device '%s' has no port mapping!\n", currentDev->infoName);
      continue;
    }

    uint8_t hwPin = *currentDev->drvPort->pwmPin;

	    // --- CLONE MODE: Synchronize PWM timer with parent ---
    if (currentDev->parentID) {
      uint8_t pID = *currentDev->parentID;
      if (pID < config.dcDevCount) {
        dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
        dcDevObj[i].attach(hwPin);
        Serial.printf("[DRV] ID:%d (Clone) attached to Pin:%d (Sync with Parent:%d)\n", currentDev->ID, hwPin, pID);
      }
    }

	    // --- MASTER MODE: Independent setup ---
    else {
      if (currentDev->pwmFreq) {
        dcDevObj[i].attach(hwPin, -1, *currentDev->pwmFreq);
        Serial.printf("[DRV] ID:%d (Master) attached to Pin:%d at %u Hz\n", currentDev->ID, hwPin, *currentDev->pwmFreq);
      } else {
        dcDevObj[i].attach(hwPin);
        Serial.printf("[DRV] ID:%d (Master) attached to Pin:%d (Default Freq)\n", currentDev->ID, hwPin);
      }
    }

	    // --- HARDWARE WAKE-UP (DRV8801 specific) ---
	    // Sets Sleep and Enable pins to wake up the physical bridge
    if (currentDev->drvPort->slpPin) {
      pinMode(*currentDev->drvPort->slpPin, OUTPUT);
      digitalWrite(*currentDev->drvPort->slpPin, HIGH); // Wake up from sleep
    }
    if (currentDev->drvPort->enPin) {
      pinMode(*currentDev->drvPort->enPin, OUTPUT);
      digitalWrite(*currentDev->drvPort->enPin, HIGH); // Enable driver
    }

    dcDevObj[i].stop();
  }
}

// EOF hw_init_drv.cpp
