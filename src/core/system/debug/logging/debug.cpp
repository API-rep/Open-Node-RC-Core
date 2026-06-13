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
 * @brief Initialize debug serial monitor.
 *
 * @details Called automatically before setup() via a static constructor.
 *   Safe to call manually as well — the internal guard prevents double init.
 *   When LogLevel == LogNone, compiles to a near-no-op (guard flag only).
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


// =============================================================================
// 2. AUTO INIT
// =============================================================================

/**
 * @brief Automatic serial debugger core initialisation
 *
 * @details This mechanism ensures that the debug serial monitor is initialized 
 *   automatically before any debug log is emitted. It's called early and will
 *   run even if the first call-site is in setup() or before.
 * 
 *   C++ guarantees that global static objects are constructed before
 *   main() (and therefore before Arduino setup()). Declaring a static instance
 *   of a struct whose constructor calls debugInit() exploits this guarantee as
 *   a zero-cost automatic trigger.
 *
 *   The anonymous namespace confines the struct to this translation unit.
 *   It is the file-scope equivalent of marking the struct static — the name
 *   DebugAutoInit is invisible from any other .cpp file.
 *
 *   debugInit() itself is idempotent (sDebugIsInit guard), so a manual call
 *   from setup() if ever added would be harmless.
 */

namespace {
  struct DebugAutoInit {
    DebugAutoInit() { debugInit(); }
  };

  static DebugAutoInit sAutoInit;
} // namespace (anonymous)

// EOF debug.cpp
