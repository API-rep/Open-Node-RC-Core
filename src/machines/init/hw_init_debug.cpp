/*****************************************************************************
 * @file hw_init_debug.cpp
 * @brief Hardware configuration diagnostic and verbose output
 *****************************************************************************/

#include "hw_init.h"

#ifdef DEBUG_HW_INIT

/**
 * @brief Print hardware configuration for debugging
 */
void debugVehicleConfig() {
  Serial.println(F("\n========================================"));
  Serial.println(F("       VEHICLE CONFIGURATION DEBUG      "));
  Serial.println(F("========================================"));
  
  Serial.printf("Machine Name : %s\n", machine.infoName);
  Serial.printf("Global Speed : FW: %.1f%% | BK: %.1f%%\n", machine.maxFwSpeed, machine.maxBackSpeed);
  Serial.printf("DC Devices   : %d\n", machine.dcDevCount);
  Serial.printf("Servo Devices: %d\n", machine.srvDevCount);
  Serial.println(F("----------------------------------------"));
  Serial.flush();

	  // --- DC Devices Debug Section ---
  if (machine.dcDev != nullptr && machine.dcDevCount > 0) {
    for (int i = 0; i < machine.dcDevCount; i++) {
      const DcDevice* d = &machine.dcDev[i];
      Serial.printf("[DC DEV #%d] %s\n", d->ID, d->infoName);
      
      if (d->drvPort != nullptr) {
        Serial.printf("  > Board Port: %s (ID: %d)\n", d->drvPort->infoName, d->drvPort->ID);
        Serial.printf("  > Pins: PWM:%d BRK:%d EN:%d SLP:%d FLT:%d\n", 
                      d->drvPort->pwmPin.value_or(-1), 
                      d->drvPort->brkPin.value_or(-1), 
                      d->drvPort->enPin.value_or(-1), 
                      d->drvPort->slpPin.value_or(-1), 
                      d->drvPort->fltPin.value_or(-1));
      }

      if (d->pwmFreq) Serial.printf("  > Config: Freq:%u Hz", *d->pwmFreq); 
      Serial.printf(" | PolInv:%s | Mode:%d\n", d->polInv ? "YES" : "NO", d->mode);

      if (d->comChannel.has_value()) Serial.printf("  > Com Ch: %d", static_cast<int>(d->comChannel.value())); 
      if (d->parentID) Serial.printf(" | Parent: %d (CLONE)", *d->parentID); 

      Serial.printf("\n  > Max Speed FW: %.1f%% | BK: %.1f%%\n", 
                    d->maxFwSpeed.value_or(100.0f), d->maxBackSpeed.value_or(100.0f));
      
      Serial.println(F("  ---"));
      Serial.flush();
    }
  }

	  // --- Servo Devices Debug Section ---
  if (machine.srvDev != nullptr && machine.srvDevCount > 0) {
    Serial.println(F("SERVO DEVICES DETAILS:"));
    for (int i = 0; i < machine.srvDevCount; i++) {
      const SrvDevice* s = &machine.srvDev[i];
      Serial.printf("[SRV DEV #%d] %s\n", s->ID, s->infoName);
      
      if (s->srvPort != nullptr) {
        Serial.printf("  > Board Port: %s | Pin: %d\n", 
                      s->srvPort->infoName, s->srvPort->pwmPin.value_or(-1));
      }

      Serial.printf("  > Com Ch: %d | PolInv: %s\n", 
                    s->comChannel.has_value() ? static_cast<int>(s->comChannel.value()) : -1, 
                    s->isInverted ? "YES" : "NO");
      
      if (s->minAngleLimit) Serial.printf("  > Angle: Min %.1f", *s->minAngleLimit);
      if (s->maxAngleLimit) Serial.printf(" | Max %.1f", *s->maxAngleLimit);
      
      Serial.printf(" | Zero Hardware: %.1f\n", s->zeroAtHwAngle.value_or(0.0f));

      if (s->maxSpeed) Serial.printf("  > Dynamics: Speed %.1f", *s->maxSpeed);
      if (s->maxAccel) Serial.printf(" | Accel %.1f", *s->maxAccel);
      
      Serial.println(F("\n  ---"));
      Serial.flush();
    }
  }
  Serial.println(F("========================================\n"));
}

#endif // DEBUG_HW_INIT

// EOF hw_init_debug.cpp
