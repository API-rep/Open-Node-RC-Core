/******************************************************************************
 * @file debug.cpp
 * @brief Serial bootstrap implementation for the debug system
 *
 * @details Uses if constexpr exclusively — no preprocessor blocks around code.
 *   When LogLevel == LogNone, the compiler strips the entire Serial.begin body.
 *****************************************************************************/

#include "debug.h"


// =============================================================================
// 1. SERIAL BOOTSTRAP
// =============================================================================

	/// Guard to ensure bootstrap runs only once across multiple calls.
static bool sDebugIsInit = false;


/**
 * @brief Initialize debug serial monitor once.
 *
 * @details if constexpr gates eliminate the Serial calls at compile time
 *   when LogLevel == LogNone, without any preprocessor guards.
 */

void debugInit() {
  if (sDebugIsInit) return;

		// --- 1. Start serial when log output is active ---
  if constexpr (LogLevel > LogNone) {
    if (!Serial.baudRate()) {
      Serial.begin(MonitorBaud);
    }

			// --- 1.1 Optional ANSI clear before first log line ---
    if constexpr (SerialAnsi && SerialClearOnInit) {
      delay(40);
      Serial.print("\033[2J\033[H");
      Serial.flush();
    }
  }

		// --- 2. Mark bootstrap as done ---
  sDebugIsInit = true;
}

// EOF debug.cpp
