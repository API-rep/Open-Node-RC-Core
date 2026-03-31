/*!****************************************************************************
 * @file  uart_struct.h
 * @brief UART port pin assignment structure.
 *
 * @details Shared between the machine env (machines/config/boards) and the
 *   sound env (sound_module/config/boards).  Both envs define their own
 *   `uartPins[]` array (one per translation environment) using this struct as
 *   the element type.  The array is indexed by UART channel number so that any
 *   init module can resolve TX / RX GPIO pin numbers with a simple table
 *   lookup instead of referencing named board constants directly.
 *******************************************************************************///
#pragma once

#include <stdint.h>


// =============================================================================
// 1. UART PIN ASSIGNMENT
// =============================================================================

	/// Physical TX / RX GPIO pin assignment for one UART channel.
struct UartPinCfg {
	int8_t tx;  ///< TX GPIO pin number (-1 = Arduino default / not used).
	int8_t rx;  ///< RX GPIO pin number (-1 = Arduino default / not used).
};

// EOF uart_struct.h
