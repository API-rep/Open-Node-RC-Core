/*****************************************************************************
 * @file hw_init_drv.cpp
 * @brief Implementation of DC drivers allocation and hardware setup
 *****************************************************************************/

#include <core/config/combus/combus.h>
#include <struct/struct.h>
#include <core/utils/debug/debug.h>

#include "hw_init_drv.h"

// =============================================================================
// 1. OBJECT ALLOCATION & POINTERS
// =============================================================================

	// --- Global pointer to DC motor object array ---
DcMotorCore* dcDevObj = nullptr;

/**
 * @brief Initialize and allocate DC driver objects in RAM
 */
void allocateDrivers(int8_t count) {
  if (count <= 0) return;

	// --- Dynamic allocation of the motor controller array ---
  dcDevObj = new DcMotorCore[count];
  hw_log_info("  [DRV] Allocated memory for count=%d DC drivers\n", count);
}

// =============================================================================
// 2. CONFIGURATION INHERITANCE
// =============================================================================

/**
 * @brief Apply parent's configuration to child drivers (Cloning logic)
 */
void applyParentConfig(const Machine &config) {
  for (int i = 0; i < config.dcDevCount; i++) {
    DcDevice* child = &config.dcDev[i];

    if (child->parentID) {
      bool parentIsFound = false;

	      // Search for matching parent ID in the device list
      for (int j = 0; j < config.dcDevCount; j++) {
        const DcDevice* parent = &config.dcDev[j];

          // --- Copy missing fields from parent to child ---
        if (parent->ID == *child->parentID) {
          if (child->DevType == DcDevType::UNDEFINED)    child->DevType      = parent->DevType;
          if (child->usage == DevUsage::UNDEFINED)       child->usage        = parent->usage;
          if (child->mode == DcDrvMode::UNDEFINED)       child->mode         = parent->mode;
          if (!child->comChannel)                        child->comChannel   = parent->comChannel;
          if (!child->pwmFreq)                           child->pwmFreq      = parent->pwmFreq;
          if (!child->maxFwSpeed)                        child->maxFwSpeed   = parent->maxFwSpeed;
          if (!child->maxBackSpeed)                      child->maxBackSpeed = parent->maxBackSpeed;

          parentIsFound = true;
          break; 
        }
      }

      if (!parentIsFound) {
        hw_log_err("FATAL: Parent ID %d not found for driver %s\n", *child->parentID, child->infoName);
        while(1);
      }
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
	hw_log_info("  [DRV] Initializing DC drivers...\n");

	  // --- 1. Safety check: ensure drivers are configured ---
  if (config.dcDev == nullptr || config.dcDevCount <= 0) {
    hw_log_info("  [DRV] No DC devices to initialize.\n");
    return;
  }

	  // --- 2.Initialize each DC driver from config ---
  for (int i = 0; i < config.dcDevCount; i++) {
    const DcDevice* currentDev = &config.dcDev[i];

	    // Skip if device has no DC driver port mapping
    if (currentDev->drvPort == nullptr || !currentDev->drvPort->pwmPin) {
        hw_log_err("    - ERROR: DRV_%d has no DC driver port mapping\n", currentDev->ID);
      continue;
    }

    uint8_t pwmPin = *currentDev->drvPort->pwmPin;
    std::optional<int8_t> dirPin = std::nullopt;
    const DriverModel* model = currentDev->drvPort->driverModel;

    if (currentDev->drvPort->dirPin) {
      dirPin = (int8_t)(*currentDev->drvPort->dirPin);
    }
    else if (currentDev->mode == DcDrvMode::ONE_WAY) {
      dirPin = -1;
    }

	    // --- 2.1 Configure driver-side safety pins BEFORE attach() ---
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

	    // CLONE MODE: Synchronize PWM timer with parent
    if (currentDev->parentID) {
      uint8_t pID = *currentDev->parentID;
      if (pID < config.dcDevCount) {
        dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
        dcDevObj[i].attach(pwmPin, dirPin);
        if (currentDev->pwmFreq) {
          hw_log_info("    > DRV_%d attached to pin %d at %uHz frequency (clone mode)\n",
                  currentDev->ID,
                  pwmPin,
                  *currentDev->pwmFreq);
        }
        else {
          hw_log_info("    > DRV_%d attached to pin %d (clone mode)\n",
                  currentDev->ID,
                  pwmPin);
          hw_log_warn("    - WARNING: DRV_%d uses default PWM frequency\n", currentDev->ID);
        }
      } else {
        hw_log_err("    - ERROR: DRV_%d clone parent ID=%d is out of range\n", currentDev->ID, pID);
        continue;
      }
    }

	    // MASTER MODE: Independent setup
    else {
      dcDevObj[i].attach(pwmPin, dirPin);
      if (currentDev->pwmFreq) {
        hw_log_info("    > DRV_%d attached to pin %d at %uHz frequency\n",
                currentDev->ID,
                pwmPin,
                *currentDev->pwmFreq);
      }
      else {
        hw_log_info("    > DRV_%d attached to pin %d\n",
                currentDev->ID,
                pwmPin);
        hw_log_warn("    - WARNING: DRV_%d uses default PWM frequency\n", currentDev->ID);
      }
    }

      // --- 3. Final safety lock after attach() ---
    dcDevObj[i].stop();
    if (currentDev->drvPort->slpPin) {
      dcDevObj[i].sleep();
    }
    if (currentDev->drvPort->enPin) {
      dcDevObj[i].disable();
    }
  }

  hw_log_info("  [DRV] DC drivers successfully initialized\n");
  hw_log_info("\n");
}

// EOF hw_init_drv.cpp
