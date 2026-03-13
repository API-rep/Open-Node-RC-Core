/******************************************************************************
 * @file main.cpp
 * @brief Main execution loop with RunLevel State Machine
 ****************************************************************************/

#include "config/config.h"
#include "init/init.h"
#include "system/utils.h"
#include <core/system/debug/debug.h>
#include <core/system/debug/dashboard.h>
#include <core/system/input/input_manager.h>
#include <core/system/vbat/vbat_sense.h>


/**
 * @brief Main Setup
 */
void setup() {
  machine_init();
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

	// --- 1.5. Battery tick (unconditional) ---
	// Called before the failsafe early-return so the dashboard always shows
	// a fresh voltage regardless of controller state or runlevel.
	// The result (any isLow transition this tick) is stored and consumed by
	// the battery state-machine block in section 3 to avoid calling tick twice.
  bool vbatChanged = vbat_tick();

	// --- 2. Input watchdog/failsafe ---
  bool inputIsDrived = false;
  for (uint8_t i = 0; i < static_cast<uint8_t>(AnalogComBusID::CH_COUNT); i++) {
    if (comBus.analogBus[i].isDrived) {
      inputIsDrived = true;
      break;
    }
  }
  if (!inputIsDrived) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(DigitalComBusID::CH_COUNT); i++) {
      if (comBus.digitalBus[i].isDrived) {
        inputIsDrived = true;
        break;
      }
    }
  }

  static bool failsafeActive = false;
  if (!inputIsDrived) {
    if (!failsafeActive) {
      sys_log_warn("[SYSTEM][SAFE] reason=no_input_source action=force_idle_and_lock\n");
      stopAllDcDrivers(machine);
      sleepAllDcDrivers(machine);
      disableAllDcDrivers(machine);
      failsafeActive = true;
    }
    comBus.runLevel = RunLevel::IDLE;
    return;  // dashboard runs on its own FreeRTOS task (Core 0)
  }
  failsafeActive = false;

	// --- 3. Ignition key derivation ---
  uint8_t keyCh = static_cast<uint8_t>(DigitalComBusID::KEY); // dedicated ignition channel
  comBus.keyOn = comBus.digitalBus[keyCh].isDrived && comBus.digitalBus[keyCh].value;

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
        sys_log_info("[SYSTEM][STATE] runlevel=IDLE msg=system_locked_press_TRIANGLE_to_start\n");
        stateTM = millis();
        stopAllDcDrivers(machine);
        wakeupAllDcDrivers(machine);
        disableAllDcDrivers(machine);
      }

        // --- 2. Transition trigger check ---
      if (comBus.keyOn) {
        sys_log_info("[SYSTEM][EVENT] input=KEY_ON action=enter_STARTING\n");
        comBus.runLevel = RunLevel::STARTING;
      }
      break;
    }

    // ---------------------------------------------------------
    case RunLevel::STARTING : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
        sys_log_info("[SYSTEM][STATE] runlevel=STARTING\n");
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
        sys_log_info("[SYSTEM][STATE] runlevel=RUNNING\n");
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
        sys_log_info("[SYSTEM][STATE] runlevel=SLEEPING\n");
        stateTM = millis();
        stopAllDcDrivers(machine);
        sleepAllDcDrivers(machine);
        disableAllDcDrivers(machine);     
      }

      if (comBus.keyOn) {
        sys_log_info("[SYSTEM][EVENT] input=KEY_ON action=rearm_from_SLEEPING\n");
        comBus.runLevel = RunLevel::STARTING;
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

	// --- 1. Battery low-state transition (tick already called above, result reused) ---
  if (vbatChanged) {
    for (uint8_t i = 0; i < vbat_channel_count(); i++) {
      if (vbat_is_low(i)) { 
        comBus.batteryIsLow = true;
        DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::BATTERY_LOW)].value = true;
        break;
      }
    }

    if (comBus.batteryIsLow) {
      comBus.runLevel = RunLevel::SLEEPING;
      sys_log_warn("[SYSTEM][SAFE] reason=low_battery action=enter_SLEEPING\n");}
  }

	// --- 2. Output dispatch (sound TX, …) ---
  output_update(comBus, failsafeActive);

	// dashboard_update() removed — handled by dedicated FreeRTOS task on Core 0.
	// See dashboard_start_task() called from dashboard_machine_setup().

} // END OF LOOP

// EOF main.cpp
