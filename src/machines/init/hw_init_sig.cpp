/*****************************************************************************
 * @file hw_init_sig.cpp
 * @brief Implementation of signal device configuration check and init.
 *****************************************************************************/

#include <core/config/combus/combus_types.h>
#include <struct/struct.h>
#include <core/system/debug/debug.h>

#include "hw_init_sig.h"


// =============================================================================
// 1. PRIVATE HELPERS
// =============================================================================

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
 *   that exactly one of analogChannel / digitalChannel is set.
 *   Returns true when at least one error is detected — halting is the
 *   caller's responsibility.
 */
bool checkSigHwConfig(const Machine &config) {
	hw_log_info("  [SIG] Signal devices config check...");
	bool hasError = false;

	if (!config.sigDev || config.sigDevCount == 0) {
		hw_log_info(" (none)\n");
		return false;
	}

	for (int i = 0; i < config.sigDevCount; i++) {
		const SigDevice* d = &config.sigDev[i];

		  // --- Validate index vs declared ID ---
		if (d->ID != i) {
			hw_log_err("\n      [SIG] CONFIG ERROR: Signal index [%d] mismatch with sigID (%d)\n",
			           i, d->ID);
			hasError = true;
		}

		  // --- Validate that exactly one channel is assigned ---
		bool hasAnalog  = d->analogChannel.has_value();
		bool hasDigital = d->digitalChannel.has_value();

		if (!hasAnalog && !hasDigital) {
			hw_log_err("\n      [SIG] CONFIG ERROR: SIG_%d (%s) has no channel assigned\n",
			           d->ID, d->infoName ? d->infoName : "?");
			hasError = true;
		} else if (hasAnalog && hasDigital) {
			hw_log_err("\n      [SIG] CONFIG ERROR: SIG_%d (%s) has both analog and digital channels set\n",
			           d->ID, d->infoName ? d->infoName : "?");
			hasError = true;
		}
	}

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
 * @details Signal devices have no hardware driver object — this step logs
 *   each configured entry for debug traceability.
 */
void sigDevInit(const Machine &config) {
	hw_log_info("    [SIG] Initializing signal devices...\n");

	if (!config.sigDev || config.sigDevCount == 0) {
		hw_log_info("    [SIG] No signal devices to initialize.\n");
		return;
	}

	for (int i = 0; i < config.sigDevCount; i++) {
		const SigDevice* d    = &config.sigDev[i];
		const char*      name = d->infoName ? d->infoName : "?";

		if (d->digitalChannel.has_value()) {
			int8_t ch = static_cast<int8_t>(d->digitalChannel.value());
			hw_log_info("      > SIG_%d  %-32s  D:%-2d  %s\n",
			            d->ID, name, ch, sigUsageStr(d->usage));
		} else if (d->analogChannel.has_value()) {
			int8_t ch = static_cast<int8_t>(d->analogChannel.value());
			hw_log_info("      > SIG_%d  %-32s  A:%-2d  %s\n",
			            d->ID, name, ch, sigUsageStr(d->usage));
		} else {
			hw_log_warn("      [SIG] WARNING: SIG_%d (%s) has no channel assigned\n",
			            d->ID, name);
		}
	}

	hw_log_info("    [SIG] Signal devices initialized\n");
}

// EOF hw_init_sig.cpp
