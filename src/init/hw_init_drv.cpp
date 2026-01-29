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

    if (child->parentID != NOT_SET) {
      bool parentIsFound = false;

        // look for matching parent ID
      for (int j = 0; j < config.dcDevCount; j++) {
        const DcDevice* parent = &config.dcDev[j];

        if (parent->ID == child->parentID) {
            // Copy unset field
          if (child->DevType == DcDevType::UNDEFINED)    child->DevType = parent->DevType;
          if (child->usage == DevUsage::UNDEFINED)       child->usage = parent->usage;
          if (child->mode == DcDrvMode::UNDEFINED)       child->mode = parent->mode;
          if (child->comChannel == NOT_SET)              child->comChannel = parent->comChannel;
          if (child->pwmFreq == NOT_SET)                 child->pwmFreq = parent->pwmFreq;
                                                         child->maxFwSpeed = parent->maxFwSpeed;
                                                         child->maxBackSpeed = parent->maxBackSpeed;
          parentIsFound = true;
          
          break; 
        }
      }

      if (!parentIsFound) {
        Serial.printf(PSTR("SYSTEM HALTED : Critical error in configuration for DC driver %s config file\n"), child->ID);
        Serial.printf(PSTR("No matching parent ID found\n"), child->ID);
        
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

    for (int i = 0; i < config.dcDevCount; i++) {
        const DcDevice* currentDev = &config.dcDev[i];

        // 2. Critical Check: Is the device mapped to a physical board port?
        if (currentDev->drvPort == nullptr) {
            Serial.printf("[DRV] FATAL: Device '%s' (ID:%d) has no hardware port mapping!\n", 
                          currentDev->infoName, currentDev->ID);
            continue; // Skip this device to avoid crash
        }

        // Get the physical PWM pin from the mapped board port
        int8_t hwPin = currentDev->drvPort->pwmPin;

        // 3. Case 1: CLONE MODE (Uses parent's PWM timer to ensure sync)
        if (currentDev->parentID != NOT_SET) {
            int8_t pID = currentDev->parentID;

            // Security: ensure parent ID is within valid array bounds
            if (pID >= 0 && pID < config.dcDevCount) {
                // Attach physical pin and synchronize timer with parent object
                dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
                dcDevObj[i].attach(hwPin);
                
                Serial.printf("[DRV] ID:%d (Clone) attached to Port:%s (Pin:%d) - Linked to Parent:%d\n", 
                              currentDev->ID, currentDev->drvPort->infoName, hwPin, pID);
            } else {
                Serial.printf("[DRV] ERROR: Invalid Parent ID %d for Device %d\n", pID, i);
            }
        }
        // 4. Case 2: MASTER MODE (Independent settings)
        else {
            // Attach pin with the device-specific frequency
            dcDevObj[i].attach(hwPin, -1, currentDev->pwmFreq);
            
            Serial.printf("[DRV] ID:%d (Master) attached to Port:%s (Pin:%d) at %u Hz\n", 
                          currentDev->ID, currentDev->drvPort->infoName, hwPin, currentDev->pwmFreq);
        }

        // 5. Safety: ensure driver starts in a stopped state
        dcDevObj[i].stop();
    }
}