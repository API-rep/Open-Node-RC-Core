/*****************************************************************************
 * @file sys_init.cpp
 * @brief Implementation of system-level initialization routines
 *****************************************************************************/

#include "sys_init.h"
#include <config/config.h>
#include <core/system/debug/debug.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/combus/combus_init.h>
#include <core/system/hw/pin_reg.h>

// =============================================================================
// 1. SINGLETON
// =============================================================================

PinReg pinReg;  ///< Board-level GPIO registry for the machine environment.


// =============================================================================
// 2. SYSTEM INITIALIZATION LOGIC
// =============================================================================

/**
 * @brief System-level initialization.
 *
 * @details Placeholder for environment parsing, bus limits and sanity checks.
 *   Called from machine_init() before hardware initialization.
 */
void sys_init() {

	// --- 1. Pin registry (no deps: pure RAM init, first in sequence) ---
	pin_reg_init(pinReg, PinRegMaxEntry);

	// --- 2. Claim Serial0 in the UART pool if any debug or dashboard flag is set.
	// The duplicate guard in uart_com_init() will catch any accidental Serial0
	// claim attempt by another module.
	//
	// TODO: the debug serial and the dashboard currently use Serial directly.
	// A future refactor should route them through the NodeCom* returned here
	// (write/readByte/available wrappers) instead of calling Serial.print()
	// directly — this would complete the transport abstraction and allow serial0
	// to be reused safely by other modules.

#if defined(DEBUG_ALL)  || defined(DEBUG_SYSTEM) || defined(DEBUG_INPUT) || \
    defined(DEBUG_HW)   || defined(DEBUG_COMBUS) || defined(DEBUG_DASHBOARD)
	(void) uart_com_init(&Serial, DEBUG_MONITOR_BAUD, -1, -1, "debug", &pinReg);
#endif

  sys_log_info("[SYSTEM] System initialisation ...\n");

	// --- 3. Internal environment data parsing ---
	// Logic using comBus or bus limits goes here

	// --- 4. System sanity checks ---
  combus_init(static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
              static_cast<uint8_t>(DigitalComBusID::CH_COUNT));

  sys_log_info("[SYSTEM] System initialisation complete\n\n");
}

// EOF sys_init.cpp
