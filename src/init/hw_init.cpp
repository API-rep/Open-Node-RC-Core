/**
 * @file hw_init.cpp
 * @brief Hardware initialisation functions
 * This function initalize machine hardware componnent from config file.
 * Functions have to be used in setup() section of the main file.
 */

#include "hw_init.h"


void machine_hardware_setup() {
    Serial.println(F("[INIT] Starting Hardware Setup..."));
      // 1. apply user config to config files
    for (int i = 0; i < machine.dcDevCount; i++) {
        // machine.dcPorts[i] est l'instance de DriverPort pour chaque moteur
//      dcDriverPortInit(*(dcDevArray[i].drvPort)); 
    }

      // 2. Préparation des données (Héritage)
    applyParentConfig(const_cast<Machine&>(machine));


      // 4. On crée physiquement les objets moteurs en RAM
      // On utilise maintenant la valeur exacte définie dans la config
    allocateDrivers(machine.dcDevCount);


      // 5. Initialise le matériel
    dcDriverInit(machine);

    //  // 3. Initialisation des Servos
    //  if (!init_servos(machine)) {
    //      Serial.println(F("[FATAL] Servos Init Failed"));
    //      while(1);
    //  }

    Serial.println(F("[INIT] Hardware Setup Complete."));
}





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

  // --- DC DEVICES DEBUG ---
  if (machine.dcDev != nullptr && machine.dcDevCount > 0) {
    for (int i = 0; i < machine.dcDevCount; i++) {
      const DcDevice* d = &machine.dcDev[i];
      
      Serial.printf("[DC DEV #%d] %s\n", d->ID, d->infoName);
      
      // Hardware Mapping (from drvPort)
      if (d->drvPort != nullptr) {
        Serial.printf("  > Board Port: %s (ID: %d)\n", d->drvPort->infoName, d->drvPort->ID);
        Serial.printf("  > Pins: PWM:%d BRK:%d EN:%d SLP:%d FLT:%d\n", 
                      d->drvPort->pwmPin, d->drvPort->brkPin, d->drvPort->enPin, d->drvPort->slpPin, d->drvPort->fltPin);
      } else {
        Serial.println(F("  > Hardware: NOT MAPPED (Check board config)"));
      }

      // Logic Settings
      if (d->pwmFreq != (uint32_t)NOT_SET) Serial.printf("  > Config: Freq:%u Hz", d->pwmFreq); else Serial.print(F("  > Config: Freq:NOT SET"));
      Serial.printf(" | PolInv:%s | Mode:%d\n", d->polInv ? "YES" : "NO", d->mode);

      // Routing & Limits
      if (d->comChannel != NOT_SET) Serial.printf("  > Com Ch: %d", d->comChannel); else Serial.print(F("  > Com Ch: NOT SET"));
      if (d->parentID != NOT_SET) Serial.printf(" | Parent: %d (CLONE)", d->parentID); else Serial.print(F(" | Parent: NONE (MASTER)"));
      Serial.printf("\n  > Max Speed FW: %.1f%% | BK: %.1f%%\n", d->maxFwSpeed, d->maxBackSpeed);
      
      Serial.println(F("  ---"));
      Serial.flush();
      delay(10);
    }
  }

  // --- SERVO DEVICES DEBUG ---
  if (machine.srvDev != nullptr && machine.srvDevCount > 0) {
    Serial.println(F("SERVO DEVICES DETAILS:"));
    for (int i = 0; i < machine.srvDevCount; i++) {
      const SrvDevice* s = &machine.srvDev[i];
      
      Serial.printf("[SRV DEV #%d] %s\n", s->ID, s->infoName);
      
      // Hardware Mapping (from drvPort)
      if (s->srvPort != nullptr) {
        Serial.printf("  > Board Port: %s | Pin: %d\n", s->srvPort->infoName, s->srvPort->pwmPin);
      } else {
        Serial.println(F("  > Hardware: NOT MAPPED"));
      }

      // Settings & Limits
      Serial.printf("  > Com Ch: %d | PolInv: %s\n", s->comChannel, s->isInverted ? "YES" : "NO");
      
      if (s->minAngleLimit != NOT_SET) Serial.printf("  > Angle: Min %.1f", s->minAngleLimit); else Serial.print(F("  > Angle: Min NOT SET"));
      if (s->maxAngleLimit != NOT_SET) Serial.printf(" | Max %.1f", s->maxAngleLimit); else Serial.print(F(" | Max NOT SET"));
      Serial.printf(" | Zero Hardware: %.1f\n", s->zeroAtHwAngle);

      // Dynamics & Hierarchy
      if (s->maxSpeed != NOT_SET) Serial.printf("  > Dynamics: Speed %.1f", s->maxSpeed); else Serial.print(F("  > Dynamics: Speed NOT SET"));
      if (s->maxAccel != NOT_SET) Serial.printf(" | Accel %.1f", s->maxAccel); else Serial.print(F(" | Accel NOT SET"));
      
      if (s->parentID != NOT_SET) Serial.printf("\n  > Parent: %d (CLONE)", s->parentID);
      
      Serial.println(F("\n  ---"));
      Serial.flush();
      delay(10);
    }
  }

  Serial.println(F("========================================\n"));
}





  // check harware config file coherence
void checkHwConfig() {
  bool configError = false;

    // dc driver section
  for (int i = 0; i < machine.dcDevCount; i++) {
      // check driver index order vs driver config array order
    if (machine.dcDev[i].ID != i) {
      Serial.printf("!!! CONFIG ERROR : Driver index [%d] don't match driverID (%d)\n", 
                    i, machine.dcDev[i].ID);
      configError = true;
    }
    /* from hw init.h-> dcDriverInit
      put section // Security: ensure parent ID is within valid array bounds
      +++ check that parent is not a chilf of another driver*/
  }

if (configError) {
    Serial.printf(PSTR("SYSTEM HALTED : Critical error in configuration for %s config file\n"), machine.infoName);
    while(1);
  }
}