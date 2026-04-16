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
#include <core/system/output/output_manager.h>
#include <core/system/combus/combus_manager.h>
#include <core/system/combus/combus_access.h>
#include <core/system/vbat/vbat_sense.h>
#include <core/system/hw/motion.h>


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
  bool vbatChanged = vbat_sense_tick();

	// --- 2. Input watchdog/failsafe ---
  combus_watchdog(comBus, ComBusDisconnectTimeoutMs);

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
    combus_set_runlevel(comBus, RunLevel::IDLE, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
    return;  // dashboard runs on its own FreeRTOS task (Core 0)
  }
  failsafeActive = false;

	// --- 3. Ignition key derivation ---
  uint8_t keyCh = static_cast<uint8_t>(DigitalComBusID::KEY); // dedicated ignition channel
  combus_set_keyon(comBus, comBus.digitalBus[keyCh].isDrived && comBus.digitalBus[keyCh].value, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));

// =============================================================================
// 2. RUNLEVEL STATE MACHINE
// =============================================================================

	// --- 1. RunLevel tracking and timing tokens ---
  static RunLevel lastRunLevel = RunLevel::NOT_YET_SET;
  static uint32_t stateTM      = 0;

	// --- 2. RunLevel change detection ---
  bool isNewRunLevel = (comBus.runLevel != lastRunLevel);
  lastRunLevel = comBus.runLevel;  // Capture BEFORE switch — prevents mid-loop transitions from masking isNewRunLevel

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
        combus_set_runlevel(comBus, RunLevel::STARTING, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
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
        combus_set_runlevel(comBus, RunLevel::RUNNING, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
      break;
    }

    // ---------------------------------------------------------
    case RunLevel::RUNNING : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
        sys_log_info("[SYSTEM][STATE] runlevel=RUNNING\n");
        stateTM = millis();
        stopAllDcDrivers(machine);
#ifdef PATCH_MOTORS_FORCE_SLEEP
        sys_log_warn("[PATCH] PATCH_MOTORS_FORCE_SLEEP active — all DC drivers forced to sleep\n");
        sleepAllDcDrivers(machine);
        disableAllDcDrivers(machine);
#else
        wakeupAllDcDrivers(machine);
        enableAllDcDrivers(machine);
#endif
      }

        // --- 1. Process DC Drivers ---
      for (int i = 0; i < machine.dcDevCount; i++) {
        uint8_t  chIdx    = static_cast<uint8_t>(machine.dcDev[i].comChannel.value());
        uint16_t busVal   = comBus.analogBus[chIdx].value;  ///< read-only — ComBus is never modified here

        uint16_t motorCmd = comBus.analogBus[chIdx].isDrived
                          ? busVal
                          : (machine.dcDev[i].signal == DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER
                          || machine.dcDev[i].signal == DcDrvSignal::SERVO_SIG_NEUTRAL_CENTER)
                          ? (comBus.analogBusMaxVal / 2) : 0u;

          // --- Motion filter (applied to motorCmd only — busVal stays untouched) ---
        if (machine.dcDev[i].motion != nullptr) {
          motion_update(motorCmd, machine.dcDev[i].motion, &machine.dcDev[i].motionRt, nullptr);
          motorCmd = machine.dcDev[i].motionRt.currentPos;
          // NOTE: filtered value is intentionally NOT written back to ENGINE_RPM_BUS.
          //       The sound module runs its own ESC inertia simulation on the raw
          //       throttle value; feeding a pre-filtered signal would double the ramp
          //       and prevent the engine RPM sound from tracking vehicle speed.
          //       ESC_SPEED_BUS is written separately below for gear-shift logic only.
          if (chIdx == static_cast<uint8_t>(AnalogComBusID::ENGINE_RPM_BUS)) {
            // Publish real inertia-filtered speed for the sound node's gear-shift
            // decisions. No double-ramp risk: sound uses ENGINE_RPM_BUS for RPM
            // pitch; ESC_SPEED_BUS is consumed only by the SEMI_AUTOMATIC logic.
            combus_set_analog(comBus, AnalogComBusID::ESC_SPEED_BUS,
                              machine.dcDev[i].motionRt.currentPos, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
          }
        }

#ifndef PATCH_MOTORS_FORCE_SLEEP
        float finalSpeed = (float)map(motorCmd, 0, comBus.analogBusMaxVal, -PERCENT_MAX, PERCENT_MAX);
        if (machine.dcDev[i].polInv) finalSpeed = -finalSpeed;
        dcDevObj[i].runAtSpeed(finalSpeed);
#endif
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
        combus_set_runlevel(comBus, RunLevel::STARTING, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
      }
      break;
    }

    default:
      break;
  }
  
// =============================================================================
// 3. SYSTEM TASKS (Battery, etc.)
// =============================================================================

	// --- 1. Battery low-state transition (tick already called above, result reused) ---
  if (vbatChanged) {
    for (uint8_t i = 0; i < vbat_channel_count(); i++) {
      if (vbat_is_low(i)) { 
        combus_set_battlow(comBus, true, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_VBAT));
        combus_set_digital(comBus, DigitalComBusID::BATTERY_LOW, true, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_VBAT));
        break;
      }
    }

    if (comBus.batteryIsLow) {
        combus_set_runlevel(comBus, RunLevel::SLEEPING, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
      sys_log_warn("[SYSTEM][SAFE] reason=low_battery action=enter_SLEEPING\n");}
  }

	// --- 2. Output dispatch (sound TX, …) ---
  output_update(comBus, failsafeActive);

	// dashboard_update() removed — handled by dedicated FreeRTOS task on Core 0.
	// See dashboard_start_task() called from dashboard_machine_setup().

} // END OF LOOP

// EOF main.cpp
