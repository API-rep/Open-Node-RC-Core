/*****************************************************************************
 * @file hw_init_sig.cpp
 * @brief Implementation of signal device configuration check and init.
 *****************************************************************************/

#include <core/config/machines/combus_types.h>
#include <struct/struct.h>
#include <core/system/debug/debug.h>

#include "hw_init_sig.h"


// =============================================================================
// 1. PRIVATE HELPERS
// =============================================================================

/**
 * @brief Maps a DevUsage signal value to its short name string for debug output.
 */

static const char* sigUsageStr(DevUsage u) {
	switch (u) {
		case DevUsage::SIG_HORN:     return "SIG_HORN";
		case DevUsage::SIG_LIGHT:    return "SIG_LIGHT";
		case DevUsage::SIG_SOLENOID: return "SIG_SOLENOID";
		case DevUsage::UNDEFINED: default: return "UNDEFINED";
	}
}


// =============================================================================
// 2. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify signal device configuration coherence.
 *
 * @details Checks that each signal array index matches its declared ID and
 *   that only one of combus's analogChannel or digitalChannel is set.
 *   Returns true when at least one error is detected — halting is the
 *   caller's responsibility.
 */

bool checkSigHwConfig(const Machine &config) {
	hw_log_info("  [SIG] Signal devices config check...");
	bool hasError = false;

	  // 1. Early-out: no signal devices declared.
	if (!config.sigDev || config.sigDevCount == 0) {
		hw_log_info(" (none)\n");
		return false;
	}

	  // 2. Iterate every declared signal device and apply validation rules.
	for (int i = 0; i < config.sigDevCount; i++) {
		const SigDevice* sigDev = &config.sigDev[i];

		  // 2.1 Array index must match the declared device ID.
		if (sigDev->ID != i) {
			hw_log_err("\n      [SIG] CONFIG ERROR: Signal index [%d] mismatch with sigID (%d)\n",
			           i, sigDev->ID);
			hasError = true;
		}

		  // 2.2 Exactly one channel (analog XOR digital) must be assigned.
		bool hasAnalog  = sigDev->analogChannel.has_value();
		bool hasDigital = sigDev->digitalChannel.has_value();

      // no channel assigned
		if (!hasAnalog && !hasDigital) {
			hw_log_err("\n      [SIG] CONFIG ERROR: SIG_%d (%s) has no channel assigned\n",
			           sigDev->ID, sigDev->infoName ? sigDev->infoName : "?");
			hasError = true;
		}
      // both channel types assigned
		else if (hasAnalog && hasDigital) {
			hw_log_err("\n      [SIG] CONFIG ERROR: SIG_%d (%s) has both analog and digital channels set\n",
			           sigDev->ID, sigDev->infoName ? sigDev->infoName : "?");
			hasError = true;
		}
	}

	  // 3. Report overall result.
	if (!hasError) {
		hw_log_info(" OK\n");
	}

	return hasError;
}


// =============================================================================
// 3. INITIALIZATION
// =============================================================================

/**
 * @brief Log and validate signal devices from machine configuration.
 *
 * @details Signal devices carry no hardware object — this step logs each
 *   configured entry for debug traceability and confirms the channel type
 *   (analog / digital) resolved for each signal source.
 */

void sigDevInit(const Machine &config) {
	hw_log_info("    [SIG] Initializing signal devices...\n");

    // 1. Early-out: nothing to do when the machine declares no signal devices.
	if (!config.sigDev || config.sigDevCount == 0) {
		hw_log_info("    [SIG] No signal devices to initialize.\n");
		return;
	}

	  // 2. Walk the signal device table and log each entry with its channel info.
	for (int i = 0; i < config.sigDevCount; i++) {
		const SigDevice* sigDev = &config.sigDev[i];
		const char*      name   = sigDev->infoName ? sigDev->infoName : "?";

		  // Log format:  "> SIG_N  <name>  D|A:<ch>  <usage>"
		if (sigDev->digitalChannel.has_value()) {
			int8_t ch = static_cast<int8_t>(sigDev->digitalChannel.value());
			hw_log_info("      > SIG_%d  %-32s  D:%-2d  %s\n",
			            sigDev->ID, name, ch, sigUsageStr(sigDev->usage));
		}
		else if (sigDev->analogChannel.has_value()) {
			int8_t ch = static_cast<int8_t>(sigDev->analogChannel.value());
			hw_log_info("      > SIG_%d  %-32s  A:%-2d  %s\n",
			            sigDev->ID, name, ch, sigUsageStr(sigDev->usage));
		}
    else {
			  // Should not happen after checkSigHwConfig() passes — warn defensively.
			hw_log_warn("      [SIG] WARNING: SIG_%d (%s) has no channel assigned\n",
			            sigDev->ID, name);
		}
	}

	  // 3. Report completion.
	hw_log_info("    [SIG] Signal devices initialized\n");
}

// EOF hw_init_sig.cpp
