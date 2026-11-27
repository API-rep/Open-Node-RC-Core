/******************************************************************************
 * @file input_init.h
 * @brief Input system initialisation script
 * 
 * This module starts the command source modules of the board: each one starts
 * its backend and checks its table (input_sources_begin()).
 * 
 * This script MUST be included via the init.h umbrella file.
 * 
 * ****************************************************************************/
#pragma once


// =============================================================================
// 1. CORE DEFINITIONS & STRUCTURES
// =============================================================================

	// Base structures and constants
#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

// =============================================================================
// 2. MACHINE & BUS CONFIGURATION
// =============================================================================

	// EnvCfg selector and bus enums
#include <core/config/machines/machine_type.h>

// =============================================================================
// 3. INPUT INITIALIZATION
// =============================================================================

	/// Input system init — the command source modules (input_sources_begin()), with init-sequence log output.
void input_init();

// EOF input_init.h
