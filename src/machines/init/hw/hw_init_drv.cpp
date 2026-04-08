/*****************************************************************************
 * @file hw_init_drv.cpp
 * @brief Implementation of DC drivers allocation and hardware setup
 *****************************************************************************/

#include <core/config/combus/combus_types.h>
#include <struct/struct.h>
#include <core/system/debug/debug.h>
#include <core/system/hw/motion.h>

#include "hw_init_drv.h"

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// Global pointer to DC motor object array
DcMotorCore* dcDevObj = nullptr;


/**
 * @brief Initialize and allocate DC driver objects in RAM
 */

void allocateDrivers(int8_t count) {
  if (count <= 0) return;

    // Allocate the DC motor controller array.
  dcDevObj = new DcMotorCore[count];
  hw_log_info("    [DRV] Allocated memory for %d DC drivers\n", count);
}


// =============================================================================
// 2. CONFIGURATION INHERITANCE
// =============================================================================

/**
 * @brief Propagate parent fields to clone device entries.
 *
 * @details For every `DcDevice` with `parentID` set, copies all unset
 *   fields (UNDEFINED / nullopt / nullptr) from the named parent entry.
 *   This is the sole runtime cost of the clone pattern — one linear scan
 *   at init, zero overhead afterwards.
 *
 *   Motion rules:
 *   - `motion`   pointer IS copied — all clones share the same MotionConfig
 *     (intentional: one config, N independent ramp states).
 *   - `motionRt` is NEVER copied — each device accumulates its own ramp
 *     position independently, even when sharing a MotionConfig.
 *
 * @param config  Machine descriptor owning the DC device array.
 */

void applyParentConfig(const Machine &config) {
  for (int i = 0; i < config.dcDevCount; i++) {
    DcDevice* child = &config.dcDev[i];

      // 1. Skip devices that declare no parent — nothing to inherit.
    if (!child->parentID) continue;

    bool parentIsFound = false;

      // 2. Search the device table for the matching parent entry.
    for (int j = 0; j < config.dcDevCount; j++) {
      const DcDevice* parent = &config.dcDev[j];

        // Copy unset fields from parent to child (UNDEFINED / nullopt = not set).
      if (parent->ID == *child->parentID) {
        if (child->DevType == DcDevType::UNDEFINED)    child->DevType      = parent->DevType;
        if (child->usage == DevUsage::UNDEFINED)       child->usage        = parent->usage;
        if (child->signal == DcDrvSignal::UNDEFINED)     child->signal       = parent->signal;
        if (!child->comChannel)                        child->comChannel   = parent->comChannel;
        if (!child->pwmFreq)                           child->pwmFreq      = parent->pwmFreq;
        if (!child->maxFwSpeed)                        child->maxFwSpeed   = parent->maxFwSpeed;
        if (!child->maxBackSpeed)                      child->maxBackSpeed = parent->maxBackSpeed;
        if (child->motion == nullptr)                  child->motion       = parent->motion;
        // motionRt intentionally skipped — per-instance FSM state, never inherited

        parentIsFound = true;
        break;
      }
    }

     // 3. Parent not found — config is invalid, halt immediately.
    if (!parentIsFound) {
      hw_log_err("FATAL: Parent ID %d not found for driver %s\n", *child->parentID, child->infoName);
      while(1);
    }
  }
}


// =============================================================================
// 3. HARDWARE INITIALIZATION
// =============================================================================

/**
 * @brief Initialize DC drivers hardware from machine configuration
 */

void dcDriverInit(const Machine &config) {
	hw_log_info("    [DRV] Initializing DC drivers...\n");

    // 1. Early-out: no DC drivers configured.
  if (config.dcDev == nullptr || config.dcDevCount <= 0) {
    hw_log_info("    [DRV] No DC devices to initialize.\n");
    return;
  }

    // 2. Initialize each DC driver from config.
  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* currentDev = &config.dcDev[i];

      // 2.1 Skip if device has no DC driver port mapping.
    if (currentDev->drvPort == nullptr || !currentDev->drvPort->pwmPin) {
        hw_log_err("        [DRV] ERROR: DRV_%d has no DC driver port mapping\n", currentDev->ID);
      continue;
    }

      // 2.2 Resolve PWM pin, DIR pin, and driver model.
    uint8_t pwmPin = *currentDev->drvPort->pwmPin;
    std::optional<int8_t> dirPin = std::nullopt;
    const DriverModel* model = currentDev->drvPort->driverModel;

    if (currentDev->drvPort->dirPin) {
      dirPin = (int8_t)(*currentDev->drvPort->dirPin);
    }
    else if (currentDev->signal == DcDrvSignal::PWM_ONE_WAY) {
      dirPin = -1;
    }

      // 2.3 Configure driver-side safety pins (safe idle: sleep + disabled).
    if (currentDev->drvPort->slpPin) {
      ActiveLevel sleepMode = (model) ? model->sleepActiveLevel : ActiveLevel::ActiveHigh;
      dcDevObj[i].setSleepPin(*currentDev->drvPort->slpPin, sleepMode);
      dcDevObj[i].sleep();
    }

    if (currentDev->drvPort->enPin) {
      ActiveLevel enableMode = (model) ? model->enableActiveLevel : ActiveLevel::ActiveHigh;
      dcDevObj[i].setEnablePin(*currentDev->drvPort->enPin, enableMode);
      dcDevObj[i].disable();
    }

    if (currentDev->drvPort->brkPin && model &&
        (model->DecayPinHighState == DecayMode::SlowDecay || model->DecayPinHighState == DecayMode::FastDecay)) {
      DecayMode highState = model->DecayPinHighState;
      DecayMode lowState = (highState == DecayMode::SlowDecay) ? DecayMode::FastDecay : DecayMode::SlowDecay;
      dcDevObj[i].setDecayPin(*currentDev->drvPort->brkPin, lowState, highState);

      if (model->defaultdDecayMode != DecayMode::Unset) {
        dcDevObj[i].decayMode(model->defaultdDecayMode);
      }
    }

    if (currentDev->pwmFreq) {
      dcDevObj[i].setPwmFreq(*currentDev->pwmFreq);
    }

      // 2.4 Attach to PWM pin
      // 2.4.1 Clone mode (shared timer) or master mode.
    if (currentDev->parentID) {
      uint8_t pID = *currentDev->parentID;
      if (pID < config.dcDevCount) {
        dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
        dcDevObj[i].attach(pwmPin, dirPin);
        if (currentDev->pwmFreq) {
          hw_log_info("      > DRV_%d attached to pin %d at %uHz frequency (clone mode)\n",
                  currentDev->ID,
                  pwmPin,
                  *currentDev->pwmFreq);
        }
        else {
          hw_log_info("      > DRV_%d attached to pin %d (clone mode)\n",
                  currentDev->ID,
                  pwmPin);
          hw_log_warn("      [DRV] WARNING: DRV_%d uses default PWM frequency\n", currentDev->ID);
        }
      } else {
        hw_log_err("      [DRV] ERROR: DRV_%d clone parent ID=%d is out of range\n", currentDev->ID, pID);
        continue;
      }
    }

      // 2.4.2 Master mode: independent timer
    else {
      dcDevObj[i].attach(pwmPin, dirPin);
      if (currentDev->pwmFreq) {
        hw_log_info("      > DRV_%d attached to pin %d at %uHz frequency\n",
                currentDev->ID,
                pwmPin,
                *currentDev->pwmFreq);
      }
      else {
        hw_log_info("      > DRV_%d attached to pin %d\n",
                currentDev->ID,
                pwmPin);
        hw_log_warn("      [DRV] WARNING: DRV_%d uses default PWM frequency\n", currentDev->ID);
      }
    }

  }

    // 3. Report completion.
  hw_log_info("    [DRV] DC drivers successfully initialized\n");
}


// =============================================================================
// 4. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify DC driver configuration coherence.
 *
 * @details Checks that each driver array index matches its declared ID.
 *   Returns true when at least one error is detected \u2014 halting is the
 *   caller's responsibility.
 */
bool checkDrvHwConfig(const Machine &config) {
  hw_log_info("  [DRV] DC drivers config check...");
  bool hasError = false;

    // 1. Validate driver index vs declared ID.
  for (int i = 0; i < config.dcDevCount; i++) {
    if (config.dcDev[i].ID != i) {
      hw_log_err("\n      [DRV] CONFIG ERROR: Driver index [%d] mismatch with driverID (%d)\n",
                 i, config.dcDev[i].ID);
      hasError = true;
    }
  }

    // 2. Validate motion config for every device that declares one.
  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* dev = &config.dcDev[i];
    if (dev->motion == nullptr) continue;
    if (!motion_check(dev->motion)) {
      hw_log_err("\n      [DRV] CONFIG ERROR: motion_check failed for driver %s\n", dev->infoName);
      hasError = true;
    }
  }

    // 3. Report overall result.
  if (!hasError) {
    hw_log_info(" OK\n");
  }

  return hasError;
}

// EOF hw_init_drv.cpp
