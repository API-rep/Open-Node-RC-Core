/******************************************************************************
 * @file main.cpp
 * @brief Main execution loop with RunLevel State EnvCfg
 ****************************************************************************/

#include "config/config.h"
#include "init/init.h"
#include "system/utils.h"
#include <core/config/inputs/PS4_dualshock.h>  // DigitalInputDevID enum
#include <core/system/debug/logging/debug.h>
#include <core/system/debug/dashboard/dashboard.h>
#include <core/system/output/output_manager.h>
#include <core/system/combus/combus_access.h>
#include <core/system/combus/processors/proc_chain.h>
#include <core/system/vbat/vbat_sense.h>

#include <machines/system/sys_manager.h>
#include <combus_remote_ids.h>   // AnalogComBusRemoteID / DigitalComBusRemoteID (Phase 4)


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
// 1. SYSTEM TICK
// =============================================================================

	// --- Input + battery + failsafe evaluation ---
  static bool s_failsafeWasActive = false;

  sys_manager_update(comBus);

    // Open-drain failsafe: no physical input source refreshed the bus this
    // cycle.  Read directly from `comBus.isNotDrived` (FS1 — inverted semantics,
    // true = fault).  The open-drain invariant guarantees `sys_manager_reset()`
    // is the only writer of `true`.
  if (comBus.isNotDrived) {
    if (!s_failsafeWasActive) {
      sys_log_warn("[SYSTEM][SAFE] reason=no_input_source action=force_idle_and_lock\n");
      stopAllDcDrivers(machine);
      sleepAllDcDrivers(machine);
      disableAllDcDrivers(machine);
        // Clear KEY_ACTIVE so cb_runlevel can detect a fresh rising edge on reconnect.
        // Without this, prevValue stays true (last engine-on state) and the rising
        // edge is never re-armed — the machine would be stuck in IDLE after reconnect.
      combus_set_digital(comBus, DigitalComBusID::KEY_ACTIVE, false,
                         ChanLayer::LOCAL);
      s_failsafeWasActive = true;
    }
    // RL3: runLevel is now a plain analog channel — write RUNLEVEL=IDLE via the generic accessor.
    combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::IDLE, ChanLayer::LOCAL);
    return;  // dashboard runs on its own FreeRTOS task (Core 0)
  }
  s_failsafeWasActive = false;

// =============================================================================
// 2. INPUT CHAIN (always — before RunLevel FSM)
// =============================================================================

  proc_chain_update(machine.inputChain, machine.inputChainCount, comBus);  // btn → KEY_ACTIVE + counters (subgear, direct-drive)
  proc_chain_update(machine.simChain,   machine.simChainCount,   comBus);  // physics (gear, ramp, bypass, …) — runs unconditionally so inertia converges to neutral during IDLE

// =============================================================================
// 3. RUNLEVEL STATE MACHINE
// =============================================================================

	// --- 1. RunLevel tracking and timing tokens ---
  // RL3: lastRunLevel reads from the generic analog channel RUNLEVEL (no dedicated struct field).
  static RunLevel lastRunLevel = RunLevel::NOT_YET_SET;
  static uint32_t stateTM      = 0;

	// --- 2. RunLevel change detection ---
  const RunLevel curRunLevel = (RunLevel)comBus.analogBus[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)].value;
  bool isNewRunLevel = (curRunLevel != lastRunLevel);
  lastRunLevel = curRunLevel;  // Capture BEFORE switch — prevents mid-loop transitions from masking isNewRunLevel

	// --- 3. RunLevel Execution ---
  switch (curRunLevel) {

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
          // RL3: generic analog write via RUNLEVEL channel.
          combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::SLEEPING, ChanLayer::LOCAL);
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
        // RL3: generic analog write via RUNLEVEL channel.
        combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::RUNNING, ChanLayer::LOCAL);
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
        // Phase 4 (A14) + Phase 5 (A15): each TYPE has its own 0-based
        // counter, so CH_COUNT is the exact count per type — no
        // subtraction needed.
        constexpr uint8_t kRemoteAnalogCount =
            static_cast<uint8_t>(AnalogComBusRemoteID::CH_COUNT);
        constexpr uint8_t kRemoteDigitalCount =
            static_cast<uint8_t>(DigitalComBusRemoteID::CH_COUNT);
        bool active = false;
        for (uint8_t i = 0; i < kRemoteAnalogCount && !active; i++) {
            const int32_t off = static_cast<int32_t>(comBus.analogBus[i].value) - static_cast<int32_t>(CbusNeutral);
            if (off > kIdleBand || off < -kIdleBand) active = true;
        }
        for (uint8_t i = 0; i < kRemoteDigitalCount && !active; i++) {
            if (comBus.digitalBus[i].value) active = true;
        }
        if (active) s_lastActivityMs = millis();
        if (millis() - s_lastActivityMs >= kEngineOffTimeoutMs) {
            sys_log_info("[SYSTEM][EVENT] reason=idle_timeout action=enter_IDLE\n");
            // RL3: generic analog write via RUNLEVEL channel.
            combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::IDLE, ChanLayer::LOCAL);
            break;
        }
      }

        // --- 1. Process DC Drivers ---
      for (int i = 0; i < machine.dcDevCount; i++) {
        uint8_t  chIdx    = static_cast<uint8_t>(machine.dcDev[i].comChannel.value());
        uint16_t busVal   = comBus.analogBus[chIdx].value;  ///< read-only — ComBus is never modified here

        uint16_t motorCmd = busVal;   // isDrived always true when RUNNING is reached (failsafe returned above)

#ifndef PATCH_MOTORS_FORCE_SLEEP
        float finalSpeed = (float)map(motorCmd, 0, comBus.analogBusMaxVal, -PERCENT_MAX, PERCENT_MAX);
        if (machine.dcDev[i].polInv) finalSpeed = -finalSpeed;
        dcDevObj[i].runAtSpeed(finalSpeed);
#endif
      }

      break;
    }

    // ---------------------------------------------------------
    case RunLevel::TURNING_OFF : {
    // ---------------------------------------------------------
      if (isNewRunLevel) {
        sys_log_info("[SYSTEM][STATE] runlevel=TURNING_OFF\n");
        stateTM = millis();
        stopAllDcDrivers(machine);
        wakeupAllDcDrivers(machine);
        disableAllDcDrivers(machine);
      }
        // Auto-transition to IDLE — no shutdown sequence implemented yet (winter 2026).
        // RL3: generic analog write via RUNLEVEL channel.
        combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::IDLE, ChanLayer::LOCAL);
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
// 3. SYSTEM TASKS (Failsafe reaction + output dispatch)
// =============================================================================

	// --- 1. Failsafe reaction (TRANSITIONAL) ---
  // Reads the aggregated `DigitalComBusID::FAILSAFE` ComBus channel
  // (published by failsafe_update() in sys_manager_update()) and forces
  // RunLevel::SLEEPING when high.  This is a transitory fallback — the
  // proper reaction will be a `proc_failsafe_reaction` processor
  // registered in the machine chain (see doc/WIP - Failsafe module
  // design.md §9). To be removed when that proc is in place.
  if (comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::FAILSAFE)].value) {
    if (curRunLevel != RunLevel::SLEEPING) {
      // RL3: generic analog write via RUNLEVEL channel.
      combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::SLEEPING, ChanLayer::LOCAL);
      sys_log_warn("[SYSTEM][SAFE] reason=failsafe_aggregator action=enter_SLEEPING\n");
    }
  }

	// --- 2. Output dispatch (sound TX, …) ---
  output_update(comBus, false);

	// dashboard_update() removed — handled by dedicated FreeRTOS task on Core 0.
	// See dashboard_start_task() called from dashboard_machine_setup().

} // END OF LOOP

// EOF main.cpp
