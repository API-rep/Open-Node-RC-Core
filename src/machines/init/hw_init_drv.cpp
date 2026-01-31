#include "hw_init_drv.h"

/**
 * Initialize and allocate DC drivers pointer
 */

  // Empty DC driver pointer initialization
ESP32_PWM_Motor* dcDevObj = nullptr;

  // DC driver pointer allocation
void allocateDrivers(int8_t count) {
    if (count <= 0) return;

      // dynamic allocation of a DC driver object array[]
    dcDevObj = new ESP32_PWM_Motor[count];
    
    Serial.printf("[SYSTEM] Allocated memory for %d DC drivers\n", count);
}



/**
 * Apply DC driver parent's configutation to child
 */

void applyParentConfig(Machine &config) {
  for (int i = 0; i < config.dcDevCount; i++) {
      // get current element address
    DcDevice* child = &config.dcDev[i];

    if (child->parentID) {
      bool parentIsFound = false;

        // look for matching parent ID
      for (int j = 0; j < config.dcDevCount; j++) {
        const DcDevice* parent = &config.dcDev[j];

        if (parent->ID == *child->parentID) {
            // Copy unset field
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
        Serial.printf(PSTR("SYSTEM HALTED : No matching parent ID %d found for driver %s\n"), *child->parentID, child->infoName);
        
      while(1);
      }
    }
  }
}



//      /**
//       * Initialize DC drivers module type
//       */
//      
//      void dcDriverPortInit(const_cast<DriverPort&>(machine.dcDev->drvPort[i])) {
//          
//          // 1. Maximum priority to compile flag -D, then DC_DRIVER_MODEL #defined in config.h
//          #ifdef DC_DRIVER_MODEL
//            if (port.driverModel == nullptr) {
//              port.driverModel = &DC_DRIVER_MODEL;
//            }
//          // 2. Error if no DC_DRIVER_MODEl specified and no port.driverModel set in hw config file)
//          #elif
//              if (port.driverModel == nullptr) {
//                Serial.printf("Warning: Port %d n'a pas de driver configuré !\n", port.ID);
//              }
//          #endif
//      }



/**
 * Initialize DC drivers defined for the machine configuration
 */

void dcDriverInit(const Machine &config) {
    // 1. Safety check: ensure drivers are configured
  if (config.dcDev == nullptr || config.dcDevCount <= 0) {
      Serial.println(F("[DRV] No DC devices to initialize."));
      return;
  }

    // 2. Initialize each DC driver according to its configuration
  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* currentDev = &config.dcDev[i];

      // Critical Check: Ensure the device has a valid hardware port mapping and skip if not
    if (currentDev->drvPort == nullptr || !currentDev->drvPort->pwmPin) {
      Serial.printf("[DRV] FATAL: Device '%s' (ID:%d) has no hardware port mapping or invalid pin definition!\n", currentDev->infoName, currentDev->ID);
      continue;
    }

    // Get the physical PWM pin from the mapped board port
    uint8_t hwPin = *currentDev->drvPort->pwmPin;

    // 3. (case 1) CLONE MODE (Uses parent's PWM timer to ensure sync)
    if (currentDev->parentID) {
      uint8_t pID = *currentDev->parentID;

        // Security: ensure parent ID is within valid array bounds
      if (pID >= 0 && pID < config.dcDevCount) {
          // Attach physical pin and synchronize timer with parent object
        dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
        dcDevObj[i].attach(hwPin);
        
        Serial.printf("[DRV] ID:%d (Clone) attached to Port:%s (Pin:%d) - Linked to Parent:%d\n", currentDev->ID, currentDev->drvPort->infoName, hwPin, pID);
      }
      
      else {
          Serial.printf("[DRV] ERROR: Invalid Parent ID %d for Device %d\n", pID, i);
      }
    }

    // 3. (case 2) MASTER MODE (Independent settings)
    else {
      if (currentDev->pwmFreq) {
          // Attach pin with the device-specific frequency
        dcDevObj[i].attach(hwPin, -1, *currentDev->pwmFreq);

        Serial.printf("[DRV] ID:%d (Master) attached to Port:%s (Pin:%d) at %u Hz\n", currentDev->ID, currentDev->drvPort->infoName, hwPin, currentDev->pwmFreq);
      }
      else {
          // Attach pin with default frequency
        dcDevObj[i].attach(hwPin);

        Serial.printf("[DRV] ID:%d (Master) attached to Port:%s (Pin:%d) at DEFAULT Freq\n", currentDev->ID, currentDev->drvPort->infoName, hwPin);
      }

      
    }

    // 5. Safety: ensure driver starts in a stopped state
    dcDevObj[i].stop();
  }
}