/******************************************************************************
 * @file hw_init_com.h
 * @brief Communication transport hardware initialisation.
 *
 * @details Registers the shared UART port pool that every uart_com_init() claims
 *   a slot from (the debug serial, the ComBus port). Must run before the first
 *   port is claimed: it is called at the start of sys_init().
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INIT ROUTINE
// =============================================================================

	/// Register the shared UART port pool (must precede every uart_com_init()).
void hw_init_com();

// EOF hw_init_com.h
