/******************************************************************************
 * @file main.cpp
 * @brief Main execution loop with RunLevel State Machine
 ****************************************************************************/

#include "config/config.h"
#include "init/init.h"
#include "utils/utils.h"
#include <core/funct/input/input_manager.h>

	// --- 1. Battery and state variables ---
bool vBatIsLow = false;
unsigned long vBatSenseTM = 0;

/**
 * @brief Main Setup
 */
void setup() {
  Serial.begin(115200);

	// --- 1. Hardware and Input initialization ---
  machine_hardware_setup();
  input_setup();

	// --- 2. Safety pin configuration ---
#ifdef VBAT_SENSING
  pinMode(VBAT_SENSE_PIN, INPUT);
#endif 

	// Boot safety: ensure drivers are locked
  pinMode(DRV_EN_PIN, OUTPUT);
  digitalWrite(DRV_EN_PIN, LOW);
  pinMode(DRV_SLP_PIN, OUTPUT);
  digitalWrite(DRV_SLP_PIN, HIGH);
}

/**
 * @brief Main Loop
 */
void loop() {

// =============================================================================
// 1. INPUTS UPDATE TO COMBUS
// =============================================================================

	// --- 1. Sync remote inputs with bus ---
  input_update(comBus);

// =============================================================================
// 2. RUNLEVEL STATE MACHINE
// =============================================================================

	// --- 1. RunLevel tracking and timing tokens ---
  static RunLevel lastRunLevel = RunLevel::NOT_YET_SET;
  static uint32_t stateTM      = 0;

	// --- 2. RunLevel change detection ---
  bool isNewRunLevel = (comBus.runLevel != lastRunLevel);

	// --- 3. RunLevel Execution ---
  switch (comBus.runLevel) {

    // ---------------------------------------------------------
    case RunLevel::IDLE : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
          // --- 1. Run once at startup ---
        Serial.println(F("[STATE] IDLE: System locked. Press TRIANGLE to start."));
        stateTM = millis();
        stopAllDcDrivers(machine);
        wakeupAllDcDrivers(machine);
        disableAllDcDrivers(machine);
      }

        // --- 2. Transition trigger check ---
      uint8_t triangleCh = static_cast<uint8_t>(DigitalComBusID::LIGHTS);
      if (comBus.digitalBus[triangleCh].isDrived && comBus.digitalBus[triangleCh].value) {
        Serial.println(F("[EVENT] Triangle pressed: Engaging STARTING sequence"));
        comBus.runLevel = RunLevel::STARTING;
      }
      break;
    }

    // ---------------------------------------------------------
    case RunLevel::STARTING : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
        Serial.println(F("[STATE] Entering in STARTING mode"));
        stateTM = millis();
        stopAllDcDrivers(machine);
        wakeupAllDcDrivers(machine);
        enableAllDcDrivers(machine);
      }

        // --- 1. Auto-transition to RUNNING ---
      comBus.runLevel = RunLevel::RUNNING;
      break;
    }

    // ---------------------------------------------------------
    case RunLevel::RUNNING : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
        Serial.println(F("[STATE] System RUNNING"));
        stateTM = millis();
        stopAllDcDrivers(machine);
        wakeupAllDcDrivers(machine);
        enableAllDcDrivers(machine);       
      }

        // --- 1. Process DC Drivers ---
      for (int i = 0; i < machine.dcDevCount; i++) {
        uint8_t chIdx = static_cast<uint8_t>(machine.dcDev[i].comChannel.value());
        uint16_t rawValue = comBus.analogBus[chIdx].value;

        if (!comBus.analogBus[chIdx].isDrived) {
          rawValue = (machine.dcDev[i].mode == DcDrvMode::TWO_WAY_NEUTRAL_CENTER) ? (comBus.analogBusMaxVal / 2) : 0;
        }

        float speed = (float)map(rawValue, 0, comBus.analogBusMaxVal, -PERCENT_MAX, PERCENT_MAX);
        float finalSpeed = machine.dcDev[i].polInv ? -speed : speed;
        dcDevObj[i].runAtSpeed(finalSpeed);
      }
      break;
    }

    // ---------------------------------------------------------
    case RunLevel::SLEEPING : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
        Serial.println(F("[STATE] Entering in SLEEPING mode"));
        stateTM = millis();
        stopAllDcDrivers(machine);
        sleepAllDcDrivers(machine);
        disableAllDcDrivers(machine);     
      }

      if (vBatIsLow) {
        Serial.println(F("Battery low, system halted"));
        while(true) { /* Wait for reboot */ }
      }
      break;
    }

    default:
      break;
  }
  
	// --- 4. Sync lastRunLevel ---
  lastRunLevel = comBus.runLevel;

// =============================================================================
// 3. SYSTEM TASKS (Battery, etc.)
// =============================================================================

#ifdef VBAT_SENSING
	// --- 1. Battery monitoring ---
  if ((vBatSenseTM + VBAT_SENSE_INTERVAL) <= millis()) {
    float vBatSense = analogRead(VBAT_SENSE_PIN) * 3.3 / 4095;
    vBatSenseTM = millis();
    // Logic continue...
    if (vBatSense <= MIN_VBAT_SENSE) {
      vBatIsLow = true;
      comBus.runLevel = RunLevel::SLEEPING;
    }
  }
#endif 

} // END OF LOOP

// EOF main.cpp
