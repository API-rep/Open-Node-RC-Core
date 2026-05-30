/******************************************************************************
 * @file main.cpp
 * @brief Main execution loop with RunLevel State EnvCfg
 ****************************************************************************/

#include "config/config.h"
#include "init/init.h"
#include "system/utils.h"
#include <core/config/inputs/PS4_dualshock.h>  // DigitalInputDevID enum
#include <core/system/debug/debug.h>
#include <core/system/debug/dashboard.h>
#include <core/system/input/input_manager.h>
#include <core/system/output/output_manager.h>
#include <core/system/combus/combus_manager.h>
#include <core/system/combus/combus_access.h>
#include <core/system/combus/processors/proc_chain.h>
#include <core/system/vbat/vbat_sense.h>


/**
 * @brief Main Setup
 */
void setup() {
  machine_init();
}


// =============================================================================
// PERSISTENT SIMULATION STATE
// =============================================================================

/// @brief Timestamp of the last operator activity (analog stick moved or button pressed).
static uint32_t s_lastActivityMs = 0u;

/// RUNNING → IDLE after 2 min of no stick/button input.
static constexpr uint32_t kEngineOffTimeoutMs = 2u  * 60u * 1000u;
/// IDLE → SLEEPING after 15 min in IDLE state (key off).
static constexpr uint32_t kSleepTimeoutMs     = 15u * 60u * 1000u;


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

// =============================================================================
// 2. INPUT CHAIN (always — before RunLevel FSM)
// =============================================================================

  proc_chain_update(machine.inputChain, machine.inputChainCount, comBus);  // btn → KEY_ACTIVE + counters (subgear, direct-drive)

// =============================================================================
// 3. RUNLEVEL STATE MACHINE
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

        // --- 2. Sleep timeout: prolonged idle (key inactive) → SLEEPING ---
      const bool keyActive = comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::KEY_ACTIVE)].value;
      if (!keyActive && (millis() - stateTM >= kSleepTimeoutMs)) {
          sys_log_info("[SYSTEM][EVENT] reason=sleep_timeout action=enter_SLEEPING\n");
          combus_set_runlevel(comBus, RunLevel::SLEEPING, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
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
        stateTM          = millis();
        s_lastActivityMs = millis();   // reset idle countdown on each RUNNING entry
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

        // --- 0.5. Idle timeout: no stick/button input for kEngineOffTimeoutMs → IDLE ---
      {
        const int32_t kIdleBand = static_cast<int32_t>(CbusNeutral) / 20;  // ±5 % threshold
        bool active = false;
        for (uint8_t i = 0; i < static_cast<uint8_t>(AnalogComBusID::WIRE_END) && !active; i++) {
            const int32_t off = static_cast<int32_t>(comBus.analogBus[i].value) - static_cast<int32_t>(CbusNeutral);
            if (off > kIdleBand || off < -kIdleBand) active = true;
        }
        for (uint8_t i = 0; i < static_cast<uint8_t>(DigitalComBusID::WIRE_END) && !active; i++) {
            if (comBus.digitalBus[i].value) active = true;
        }
        if (active) s_lastActivityMs = millis();
        if (millis() - s_lastActivityMs >= kEngineOffTimeoutMs) {
            sys_log_info("[SYSTEM][EVENT] reason=idle_timeout action=enter_IDLE\n");
            combus_set_runlevel(comBus, RunLevel::IDLE, makeChanOwner(EnvNodeGroup, ComBusOwner::PROC_SYSTEM));
            break;
        }
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

#ifndef PATCH_MOTORS_FORCE_SLEEP
        float finalSpeed = (float)map(motorCmd, 0, comBus.analogBusMaxVal, -PERCENT_MAX, PERCENT_MAX);
        if (machine.dcDev[i].polInv) finalSpeed = -finalSpeed;
        dcDevObj[i].runAtSpeed(finalSpeed);
#endif
      }

      // --- 2. CbChain SIM pipeline ---
      proc_chain_update(machine.simChain,   machine.simChainCount,   comBus);  // physics (gear, ramp, bypass, …)
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
