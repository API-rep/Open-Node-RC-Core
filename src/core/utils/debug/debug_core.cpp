/******************************************************************************
 * @file debug_core.cpp
 * @brief Core debug bootstrap implementation
 *
 * @details
 * This module owns the shared debug serial monitor initialization.
 * All runtime modules using debug outputs should rely on this entry point.
 *****************************************************************************/

#include "debug_core.h"

#if DEBUG_ANY_ENABLED

#include <Arduino.h>


// =============================================================================
// 1. INTERNAL STATE
// =============================================================================

/**
 * @brief Guard flag to ensure debug core bootstrap is done only once
 */

static bool debugIsInit = false;


// =============================================================================
// 2. CORE DEBUG BOOTSTRAP
// =============================================================================

/**
 * @brief Initialize debug serial monitor once when debug is enabled
 *
 * @details
 * This routine first checks if debug serial was already initialized in this
 * process. It then checks whether Arduino Serial is already active
 * (non-zero baudrate). If needed, it starts Serial using DEBUG_MONITOR_BAUD.
 */

void debugCoreInit() {
		// --- 1. Return early when debug bootstrap is already done ---
	if (debugIsInit) {
		return;
	}

		// --- 2. Start serial only if currently not active ---
	if (!Serial.baudRate()) {
		Serial.begin(static_cast<unsigned long>(DEBUG_MONITOR_BAUD));
	}

		// --- 3. Mark debug core bootstrap as initialized ---
	debugIsInit = true;
}

#else

/**
 * @brief No-op implementation when debug is globally disabled
 */

void debugCoreInit() {
}

#endif

// EOF debug_core.cpp
