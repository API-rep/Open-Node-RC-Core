/******************************************************************************
 * @file srv_dev.cpp
 * @brief Servo device configuration check and initialization — core.
 *
 * @details Implements checkSrvHwConfig() and servoInit().
 *   servoInit() calls the check as its first step, allocates srvDevObj[],
 *   claims each board pin, applies the SrvDevice descriptor
 *   (PWM frequency, tick endpoints, hardware angle range), and de-energizes
 *   each servo (writeMicroseconds(0)) so it coasts at its current mechanical
 *   position until the control loop issues its first command.
 *   The control layer must use goToAtSpeed() for the first move to avoid
 *   a sudden jump when the physical position is unknown.
 ******************************************************************************/

#include "srv_dev.h"
#include <struct/struct.h>
#include <core/system/debug/debug.h>

#include <Arduino.h>


// =============================================================================
// 1. SERVO INSTANCE ARRAY
// =============================================================================

ServoCore* srvDevObj = nullptr;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/**
 * @brief Allocate the ServoCore object array.
 */
static void allocateServos(uint8_t count)
{
	srvDevObj = new ServoCore[count];
	hw_log_info("    [SRV] Allocated memory for %d servo devices\n", count);
}


// =============================================================================
// 3. CONFIGURATION CHECK
// =============================================================================

bool checkSrvHwConfig(const EnvCfg& config)
{
	hw_log_info("  [SRV] Servos config check...");
	bool hasError = false;

	  // 1. Early-out: no servo devices declared.
	if (!config.srvDev || config.srvDevCount == 0) {
		hw_log_info(" (none)\n");
		return false;
	}

	  // 2. Iterate every declared servo device and apply validation rules.
	for (int i = 0; i < config.srvDevCount; i++) {
		const SrvDevice* dev = &config.srvDev[i];

		  // 2.1 Array index must match the declared device ID.
		if (dev->ID != i) {
			hw_log_err("\n      [SRV] CONFIG ERROR: Servo index [%d] mismatch with srvID (%d)\n",
			           i, dev->ID);
			hasError = true;
		}

		  // 2.2 Hardware angle range must be non-zero.
		const SrvHwAngle& ang = dev->hwAngle;
		if (ang.totalRange() <= 0.0f) {
			hw_log_err("\n      [SRV] CONFIG ERROR: SRV_%d hwAngle invalid range (min=%.1f max=%.1f)\n",
			           dev->ID, ang.minHwAngle, ang.maxHwAngle);
			hasError = true;
		}

		  // 2.3 PWM tick window must be rational (min > 0 and min < max).
		if (dev->minUsTick == 0 || dev->maxUsTick <= dev->minUsTick) {
			hw_log_err("\n      [SRV] CONFIG ERROR: SRV_%d usTick invalid (min=%u max=%u)\n",
			           dev->ID, dev->minUsTick, dev->maxUsTick);
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

void servoInit(const EnvCfg& config, PinReg& reg)
{
	hw_log_info("    [SRV] Initializing servo devices...\n");

	  // --- 1. Early-out: no servo devices configured ---
	if (!config.srvDev || config.srvDevCount == 0) {
		hw_log_info("    [SRV] No servo devices to initialize.\n");
		return;
	}

	  // --- 2. Config coherence check ---
	if (checkSrvHwConfig(config)) {
		hw_log_err("    [SRV] FATAL: Invalid servo configuration — init aborted\n");
		return;
	}

	  // --- 3. Allocate servo objects ---
	allocateServos(config.srvDevCount);

	  // --- 4. Claim pin and configure each servo ---
	for (int i = 0; i < config.srvDevCount; i++) {
		const SrvDevice* dev = &config.srvDev[i];

		  // 4.1 Skip if device has no port mapping.
		if (!dev->srvPort || !dev->srvPort->pwmPin) {
			hw_log_warn("      [SRV] WARNING: SRV_%d has no servo port mapping\n", dev->ID);
			continue;
		}

		  // 4.2 Claim pin ownership before touching ServoCore.
		const uint8_t pwmPin = *dev->srvPort->pwmPin;
		const char* label    = (dev->infoName != nullptr) ? dev->infoName : "SRV";

		if (!pin_claim(reg, pwmPin, PinOwner::ServoOut, label, false)) {
			hw_log_warn("      [SRV] WARNING: SRV_%d skipped (GPIO%d already claimed)\n",
			            dev->ID, pwmPin);
			continue;
		}

		  // 4.3 Apply descriptor — PWM frequency, tick endpoints, angle range.
		if (dev->pwmFreq) {
			srvDevObj[i].setPwmFreq(*dev->pwmFreq);
		}

		if (!srvDevObj[i].setTickDuration(dev->minUsTick, dev->maxUsTick)) {
			hw_log_err("      [SRV] ERROR: SRV_%d invariant broken on tick duration (min=%u max=%u)\n",
			           dev->ID, dev->minUsTick, dev->maxUsTick);
			continue;
		}

		if (!srvDevObj[i].setHwAngles(dev->hwAngle.minHwAngle, dev->hwAngle.maxHwAngle)) {
			hw_log_err("      [SRV] ERROR: SRV_%d invariant broken on hwAngle range (min=%.1f max=%.1f)\n",
			           dev->ID, dev->hwAngle.minHwAngle, dev->hwAngle.maxHwAngle);
			continue;
		}

		  // 4.4 Attach to GPIO, then de-energize (no PWM pulse, 0 V output).
		  //     The servo coasts at its current mechanical position — safer than
		  //     any predetermined angle (centre or minimum) because the physical
		  //     position at boot is unknown.
		  //     Contract for the control layer: use goToAtSpeed() for the first
		  //     move so the servo accelerates gently from wherever it rests.
		if (!srvDevObj[i].begin(pwmPin)) {
			hw_log_err("      [SRV] ERROR: SRV_%d begin() failed on GPIO%d\n",
			           dev->ID, pwmPin);
			continue;
		}
    
      // servo disable (coast) at current position until control loop issues first command
		srvDevObj[i].writeMicroseconds(0);

		int8_t chId = dev->comChannel.has_value()
		              ? static_cast<int8_t>(dev->comChannel.value()) : -1;
		hw_log_info("      > SRV_%d attached to pin %d on com channel %d\n",
		            dev->ID, pwmPin, chId);
	}

	hw_log_info("    [SRV] Servo devices initialized\n");
}

// EOF srv_dev.cpp
