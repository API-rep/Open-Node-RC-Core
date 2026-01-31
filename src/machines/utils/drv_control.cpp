#include "drv_control.h"

#include <Arduino.h>

void sleepAllDcDrivers(const Machine &config) {
          // Check if there are any drivers to initialize
    if (config.dcDev == nullptr || config.dcDevCount <= 0) {
        Serial.println(F("[DRV] No DC drivers to initialize."));
        return;
    }

    for (int i = 0; i < config.dcDevCount; i++) {
        const DcDevice* currentDcDev = &config.dcDev[i];

            // write sleep pin on
        if (currentDcDev->drvPort->slpPin) {
            dcDevObj[i].sleep();
                
            Serial.printf("[DRV] DC driver ID %d put in sleep mode\n", currentDcDev->ID);
        }

        else {
            Serial.printf("[DRV] Driver ID %d has no sleep pin configured\n", currentDcDev->ID);
            
        }
    }
}



void wakeupDcDrivers(const Machine &config) {
          // Check if there are any drivers to initialize
    if (config.dcDev == nullptr || config.dcDevCount <= 0) {
        Serial.println(F("[DRV] No DC drivers to initialize."));
        return;
    }

    for (int i = 0; i < config.dcDevCount; i++) {
        const DcDevice* currentDcDev = &config.dcDev[i];

            // write sleep pin off
        if (currentDcDev->drvPort->slpPin) {
            dcDevObj[i].wakeup();
                
            Serial.printf("[DRV] DC driver ID %d wake up\n", currentDcDev->ID);
        }

        else {
            Serial.printf("[DRV] No valid DC driver to wake up");
            
        }
    }
}