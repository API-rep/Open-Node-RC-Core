/******************************************************************************
 * @file com_init.h
 * @brief Machine env communication transports initialisation — umbrella.
 *
 * @details Initialises all active communication transport layers based on
 * active build flags. Each transport module degrades to a no-op when its
 * flag is absent.
 *
 *   Active transports:
 *     COMBUS_UART_TX=N, COMBUS_UART=N — ComBus TX on UARTn
 *
 *   Future:
 *     (additional transports registered here as sub-modules)
 *
 *   Called from output_init() after sys init.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. COM INIT
// =============================================================================

	/// Initialise all active communication transport layers.
void com_init();

// EOF com_init.h
