#include "config/config.h"
#include "init/init.h"
#include "utils/utils.h"

#include <PS4Controller.h>

  // for PS4 controller disconect timeout
bool PS4isDisconnected = false;
unsigned long PS4_lastUpdateTM = 0;

  // for battery voltage monitoring
bool vBatIsLow = false;
unsigned long vBatSenseTM = 0;

  // runlevel tracking variable
RunLevel lastRunLevel = RunLevel::NOT_YET_SET ; 

void setup() {

  Serial.begin(115200);

  PS4.begin(PS4_BLUETOOTH_ADDRESS);
  
    // enable V-BAT readout on sense pin
  #ifdef VBAT_SENSING
    pinMode(VBAT_SENSE_PIN, INPUT);
  #endif 

    // motor configuation and set to disable durring setup for safety reason
  pinMode(DRV_EN_PIN, OUTPUT);
  digitalWrite(DRV_EN_PIN, LOW);
  
    // wake up DC driver
  pinMode(DRV_SLP_PIN, OUTPUT);
  digitalWrite(DRV_SLP_PIN, HIGH);
  
    // machine hardware initialisation
   machine_hardware_setup();
}

void loop() {

/**
 * Read PS4 controller inputs and store them into internal channels structure
 * NOTE:
 * - Put the following code into library if it grow to much
 */

  if (PS4.isConnected()) {
      // update connection time mark and status flag
    PS4isDisconnected = false;
    PS4_lastUpdateTM = millis();

      // update LStickX internal channel value
    if (PS4.LStickX()) {
      comBus.analogBus[STEERING_BUS].value = map(PS4.LStickX(), DEF_STICK_MIN_VAL, DEF_STICK_MAX_VAL, 0, comBus.analogBusMaxVal);
      comBus.analogBus[STEERING_BUS].isDrived = true;
    }
      // update LStickY internal channel value
    if (PS4.LStickY()) {
      comBus.analogBus[DRIVE_SPEED_BUS].value = map(PS4.LStickY(), DEF_STICK_MIN_VAL, DEF_STICK_MAX_VAL, 0, comBus.analogBusMaxVal);
      comBus.analogBus[DRIVE_SPEED_BUS].isDrived = true;
    }
      // update RStickY internal channel value
    if (PS4.RStickY()) {
      comBus.analogBus[DUMP_BUS].value = map(PS4.RStickY(), DEF_STICK_MIN_VAL, DEF_STICK_MAX_VAL, 0, comBus.analogBusMaxVal);
      comBus.analogBus[DUMP_BUS].isDrived = true;     
    }
  }



/**
 * Main loop is split into différents runlevels. To switch from one to another, edit "runLevel" variable
 * with the target runlevel. See const.h for a list of available runlevels.
 * Runlevel is also an easy way to share machine state with others deveices (trailer, sound module, ...).
 * Place time crytical or common task outside of switch statement
 */

  // check runlevel tracking variable
 bool isNewRunLevel = (comBus.runLevel != lastRunLevel);


  switch (comBus.runLevel) {

    case RunLevel::IDLE :
      Serial.println("Entering in IDDLE mode");

      break;

    case RunLevel::STARTING :
      Serial.println("Entering in RunLevel::STARTING mode");

      break;

    case RunLevel::RUNNING :
        // run once on runlevel change
      if (isNewRunLevel) {
        Serial.println("Entering in RUNNING mode");
        
        // Wake up and enable motors (une seule fois à l'entrée)
        digitalWrite(DRV_EN_PIN, HIGH);
        digitalWrite(DRV_SLP_PIN, HIGH);
        
        // Vous pouvez ajouter ici d'autres inits (sons, lumières...)
      }

        // update all configured DC drivers speed from comBus
      for (int i = 0; i < machine.dcDevCount; i++) {
          // raw analog bus value readed
        uint16_t rawValue = comBus.analogBus[*machine.dcDev[i].comChannel].value;

          // set DC driver in safe mode if comBus is not drived 

          // attention, à relire //
        if (!comBus.analogBus[*machine.dcDev[i].comChannel].value) {
          switch (machine.dcDev[i].mode)
          {
          case DcDrvMode::ONE_WAY:
            rawValue = 0;
            break;

          case DcDrvMode::TWO_WAY_NEUTRAL_CENTER:
            rawValue = comBus.analogBusMaxVal / 2;  // 50% duty cycle
            break;
          
          default:
            break;
          }
        }

          // speed converted from raw analog bus value to percent
        float speed = (float)map(rawValue, 0, comBus.analogBusMaxVal, -PERCENT_MAX, PERCENT_MAX);

          // final speed to write to the driver
        float finalSpeed = machine.dcDev[i].polInv ? -speed : speed;  // invert speed if drvConfigs[i].polInv is true

          // motor speed update 
        dcDevObj[i].runAtSpeed(finalSpeed);
      }

      break;

    case RunLevel::TURNING_OFF :
      Serial.println("Entering in TUNING_OFF mode");
      // do something

      break;

    case RunLevel::SLEEPING :
      Serial.println("Entering in RunLevel::SLEEPING mode");

        // think to do if low bat detection
      if (vBatIsLow) {
        Serial.println("Battery low, switch to sleeping mode");
          // disable motors
        digitalWrite(DRV_EN_PIN, LOW);
        digitalWrite(DRV_SLP_PIN, LOW);
          // disable servo
          // -> macro to write to disable servo 
      
        while(true) {
          // loop untill reboot
        }
      }

      break;

    case RunLevel::RESET :
          // code use to reset the controler
      break;

    default:
      // if nothing else matches, do the default
      break;

  }

  // update runlevel tracking varibale
lastRunLevel = comBus.runLevel;

  #ifdef VBAT_SENSING

    /**
      *  Battery monitoring
      *  Check battery voltage an switch to RunLevel::SLEEPING runLevel if low voltage is detected
    */

      // sense battery at each VBAT_SENSE_INTERVAL 
    if ((vBatSenseTM + VBAT_SENSE_INTERVAL) <= millis()) {
        // V-BAT sensing and sampling
      float vBatSense = analogRead(VBAT_SENSE_PIN) * 3.3 / 4095;
      vBatSenseTM = millis();
      
      vBatSense = ((vBatSense * VBAT_SENSE_SAMPLING) + (analogRead(VBAT_SENSE_PIN) * 3.3 / 4095)) / (VBAT_SENSE_SAMPLING + 1);
      
      if (vBatSense <= MIN_VBAT_SENSE) {
          // enable low battery voltage flag and switch to SLEEP runlevel
        vBatIsLow = true;
        comBus.runLevel = RunLevel::SLEEPING;
      }
    }
  #endif 
}
