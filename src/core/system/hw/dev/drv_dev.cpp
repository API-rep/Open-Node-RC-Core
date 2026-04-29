/******************************************************************************
 * @file drv_dev.cpp
 * @brief DC driver configuration check and initialization — core.
 *
 * @details Implements checkDrvHwConfig() and dcDriverInit().
 *   dcDriverInit() propagates clone fields (applyParentConfig), calls the
 *   check as its second step, allocates dcDevObj[], then configures each
 *   driver: PWM frequency, pin claims (DIR/FLT/SLP/EN/BRK), and attaches
 *   the PWM channel in clone or master timer mode.
 ******************************************************************************/

#include "drv_dev.h"
#include <struct/struct.h>
#include <core/system/debug/debug.h>
#include <core/system/hw/motion.h>

#include <Arduino.h>


// =============================================================================
// 1. DC DRIVER INSTANCE ARRAY
// =============================================================================

DcMotorCore* dcDevObj = nullptr;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/**
 * @brief Allocate the DcMotorCore object array.
 */
static void allocateDrivers(int8_t count)
{
	if (count <= 0) return;
	dcDevObj = new DcMotorCore[count];
	hw_log_info("    [DRV] Allocated memory for %d DC drivers\n", count);
}


/**
 * @brief Propagate parent fields to clone device entries.
 *
 * @details For every DcDevice with parentID set, copies all unset fields
 *   (UNDEFINED / nullopt / nullptr) from the named parent entry.
 *   This is the sole runtime cost of the clone pattern — one linear scan
 *   at init, zero overhead afterwards.
 *
 *   Motion rules:
 *   - motion   pointer IS copied — all clones share the same MotionConfig.
 *   - motionRt is NEVER copied — each device accumulates its own ramp state.
 *
 * @param config  EnvCfg descriptor owning the DC device array.
 */
static void applyParentConfig(const EnvCfg& config)
{
	for (int i = 0; i < config.dcDevCount; i++) {
		DcDevice* child = &config.dcDev[i];

		  // 1. Skip devices that declare no parent — nothing to inherit.
		if (!child->parentID) continue;

		bool parentIsFound = false;

		  // 2. Search the device table for the matching parent entry.
		for (int j = 0; j < config.dcDevCount; j++) {
			const DcDevice* parent = &config.dcDev[j];

			if (parent->ID == *child->parentID) {
				if (child->DevType == DcDevType::UNDEFINED)    child->DevType      = parent->DevType;
				if (child->usage == DevUsage::UNDEFINED)       child->usage        = parent->usage;
				if (child->signal == DcDrvSignal::UNDEFINED)   child->signal       = parent->signal;
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
			hw_log_err("FATAL: Parent ID %d not found for driver %s\n",
			           *child->parentID, child->infoName);
			while (1);
		}
	}
}


// =============================================================================
// 3. CONFIGURATION CHECK
// =============================================================================

bool checkDrvHwConfig(const EnvCfg& config)
{
	hw_log_info("  [DRV] DC drivers config check...");
	bool hasError = false;

	  // 1. Early-out: no DC drivers declared.
	if (!config.dcDev || config.dcDevCount == 0) {
		hw_log_info(" (none)\n");
		return false;
	}

	  // 2. Validate driver index vs declared ID.
	for (int i = 0; i < config.dcDevCount; i++) {
		if (config.dcDev[i].ID != i) {
			hw_log_err("\n      [DRV] CONFIG ERROR: Driver index [%d] mismatch with driverID (%d)\n",
			           i, config.dcDev[i].ID);
			hasError = true;
		}
	}

	  // 3. Validate motion config for every device that declares one.
	for (int i = 0; i < config.dcDevCount; i++) {
		const DcDevice* dev = &config.dcDev[i];
		if (dev->motion == nullptr) continue;
		if (!motion_check(dev->motion)) {
			hw_log_err("\n      [DRV] CONFIG ERROR: motion_check failed for driver %s\n",
			           dev->infoName);
			hasError = true;
		}
	}

	if (!hasError) {
		hw_log_info(" OK\n");
	}

	return hasError;
}


// =============================================================================
// 4. INITIALIZATION
// =============================================================================

void dcDriverInit(const EnvCfg& config, PinReg& reg)
{
	hw_log_info("    [DRV] Initializing DC drivers...\n");

	  // --- 1. Early-out: no DC drivers configured ---
	if (config.dcDev == nullptr || config.dcDevCount <= 0) {
		hw_log_info("    [DRV] No DC devices to initialize.\n");
		return;
	}

	  // --- 2. Clone inheritance ---
	applyParentConfig(config);

	  // --- 3. Config coherence check ---
	if (checkDrvHwConfig(config)) {
		hw_log_err("    [DRV] FATAL: Invalid driver configuration — init aborted\n");
		return;
	}

	  // --- 4. Allocate driver objects ---
	allocateDrivers(config.dcDevCount);

	  // --- 5. Claim pins and configure each driver ---
	for (int i = 0; i < config.dcDevCount; i++) {
		const DcDevice* currentDev = &config.dcDev[i];

		  // 5.1 Skip if device has no DC driver port mapping.
		if (currentDev->drvPort == nullptr || !currentDev->drvPort->pwmPin) {
			hw_log_err("        [DRV] ERROR: DRV_%d has no DC driver port mapping\n",
			           currentDev->ID);
			continue;
		}

		  // 5.2 Resolve PWM pin, DIR pin, and driver model.
		uint8_t pwmPin = *currentDev->drvPort->pwmPin;
		std::optional<int8_t> dirPin = std::nullopt;
		const DriverModel* model = currentDev->drvPort->driverModel;
		const char* pinLabel = (currentDev->infoName != nullptr) ? currentDev->infoName : "DRV";

		  // 5.2.1 Resolve and claim DIR pin when present.
		if (currentDev->drvPort->dirPin) {
			if (!pin_claim(reg, *currentDev->drvPort->dirPin, PinOwner::DcDrvDir, pinLabel, false, true)) {
				hw_log_warn("      [DRV] WARNING: DRV_%d skipped (DIR GPIO already claimed)\n",
				            currentDev->ID);
				continue;
			}
			dirPin = (int8_t)(*currentDev->drvPort->dirPin);
		}
		else if (currentDev->signal == DcDrvSignal::PWM_ONE_WAY) {
			dirPin = -1;
		}

		  // 5.2.2 Resolve and claim fault pin when present.
		if (currentDev->drvPort->fltPin) {
			if (!pin_claim(reg, *currentDev->drvPort->fltPin, PinOwner::DcDrvFlt, pinLabel, false, true)) {
				hw_log_warn("      [DRV] WARNING: DRV_%d skipped (FLT GPIO already claimed)\n",
				            currentDev->ID);
				continue;
			}
		}

		  // 5.2.3 Resolve and claim sleep pin when present.
		if (currentDev->drvPort->slpPin) {
			if (!pin_claim(reg, *currentDev->drvPort->slpPin, PinOwner::DcDrvSlp, pinLabel, false, true)) {
				hw_log_warn("      [DRV] WARNING: DRV_%d skipped (SLP GPIO already claimed)\n",
				            currentDev->ID);
				continue;
			}
			ActiveLevel sleepMode = (model) ? model->sleepActiveLevel : ActiveLevel::ActiveHigh;
			dcDevObj[i].setSleepPin(*currentDev->drvPort->slpPin, sleepMode);
			dcDevObj[i].sleep();
		}

		  // 5.2.4 Resolve and claim enable pin when present.
		if (currentDev->drvPort->enPin) {
			if (!pin_claim(reg, *currentDev->drvPort->enPin, PinOwner::DcDrvEn, pinLabel, false, true)) {
				hw_log_warn("      [DRV] WARNING: DRV_%d skipped (EN GPIO already claimed)\n",
				            currentDev->ID);
				continue;
			}
			ActiveLevel enableMode = (model) ? model->enableActiveLevel : ActiveLevel::ActiveHigh;
			dcDevObj[i].setEnablePin(*currentDev->drvPort->enPin, enableMode);
			dcDevObj[i].disable();
		}

		  // 5.2.5 Resolve and claim brake/decay pin when present.
		if (currentDev->drvPort->brkPin && model &&
		    (model->DecayPinHighState == DecayMode::SlowDecay ||
		     model->DecayPinHighState == DecayMode::FastDecay)) {
			if (!pin_claim(reg, *currentDev->drvPort->brkPin, PinOwner::DcDrvBrk, pinLabel, false, true)) {
				hw_log_warn("      [DRV] WARNING: DRV_%d skipped (BRK GPIO already claimed)\n",
				            currentDev->ID);
				continue;
			}
			DecayMode highState = model->DecayPinHighState;
			DecayMode lowState = (highState == DecayMode::SlowDecay)
			                     ? DecayMode::FastDecay : DecayMode::SlowDecay;
			dcDevObj[i].setDecayPin(*currentDev->drvPort->brkPin, lowState, highState);
			if (model->defaultdDecayMode != DecayMode::Unset) {
				dcDevObj[i].decayMode(model->defaultdDecayMode);
			}
		}

		if (currentDev->pwmFreq) {
			dcDevObj[i].setPwmFreq(*currentDev->pwmFreq);
		}

		  // 5.3 Claim PWM pin.
		if (!pin_claim(reg, pwmPin, PinOwner::DcDrvPwm, pinLabel, false, false)) {
			hw_log_warn("      [DRV] WARNING: DRV_%d skipped (PWM GPIO already claimed)\n",
			            currentDev->ID);
			continue;
		}

		  // 5.4 Attach to PWM channel.
		if (currentDev->parentID) {
			uint8_t pID = *currentDev->parentID;
			if (pID < config.dcDevCount) {
				dcDevObj[i].useTimer(dcDevObj[pID].getPwmTimer());
				dcDevObj[i].attach(pwmPin, dirPin);
				if (currentDev->pwmFreq) {
					hw_log_info("      > DRV_%d attached to pin %d at %uHz frequency (clone mode)\n",
					            currentDev->ID, pwmPin, *currentDev->pwmFreq);
				}
				else {
					hw_log_info("      > DRV_%d attached to pin %d (clone mode)\n",
					            currentDev->ID, pwmPin);
					hw_log_warn("      [DRV] WARNING: DRV_%d uses default PWM frequency\n",
					            currentDev->ID);
				}
			}
			else {
				hw_log_err("      [DRV] ERROR: DRV_%d clone parent ID=%d is out of range\n",
				           currentDev->ID, pID);
				continue;
			}
		}
		else {
			dcDevObj[i].attach(pwmPin, dirPin);
			if (currentDev->pwmFreq) {
				hw_log_info("      > DRV_%d attached to pin %d at %uHz frequency\n",
				            currentDev->ID, pwmPin, *currentDev->pwmFreq);
			}
			else {
				hw_log_info("      > DRV_%d attached to pin %d\n",
				            currentDev->ID, pwmPin);
				hw_log_warn("      [DRV] WARNING: DRV_%d uses default PWM frequency\n",
				            currentDev->ID);
			}
		}
	}

	hw_log_info("    [DRV] DC drivers successfully initialized\n");
}

// EOF drv_dev.cpp
