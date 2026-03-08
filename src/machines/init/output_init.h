/******************************************************************************
 * @file output_init.h
 * @brief Output peripherals initialization.
 *
 * @details Initialises all output-side transport modules (sound UART TX,
 * future CAN bus, RF link, etc.) based on active build flags.
 * Pin assignments are read from the active board configuration.
 * Compile-time cap checks live in each module’s own config sub-file
 * (e.g. outputs/sound_uart.h).
 *
 * This file MUST be included via the init.h umbrella header.
 *****************************************************************************/
#pragma once

#include <core/config/outputs/outputs.h>


// =============================================================================
// 1. OUTPUT INITIALIZATION
// =============================================================================

	/// Output peripherals init — called once from machine_init().
void output_init();

// EOF output_init.h
