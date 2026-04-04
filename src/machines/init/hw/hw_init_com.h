/******************************************************************************
 * @file hw_init_com.h
 * @brief Communication transport hardware initialisation.
 *
 * @details Opens the ComBus UART port and claims its GPIO pins before any
 *   other hardware peripheral is initialised — pin claim must be first to
 *   ensure no conflict with driver, servo, or signal pin allocations.
 *
 *   Called as the first step of hw_init().
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INIT ROUTINE
// =============================================================================

	/// Open the ComBus UART transport port (pin claim + serial.begin).
void hw_init_com();

// EOF hw_init_com.h
