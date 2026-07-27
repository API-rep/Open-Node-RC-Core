/******************************************************************************
 * @file input_init.h
 * @brief Input system initialisation script
 * 
 * This module handles the initialization of the input system, including
 * remote and mapping configuration parsing, sanity checks, and hardware
 * object creation.
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
#include <core/config/machines/machine_type_combus_ids.h>

// =============================================================================
// 3. REMOTE MAPPING STRUCTURES
// =============================================================================

	// Input device vocabulary + mapping structures
#include <core/config/inputs/inputs.h>


// =============================================================================
// 4. INPUT INITIALIZATION
// =============================================================================

	/// Input system init — input_setup() wrapper with init-sequence log output.
void input_init();

// EOF input_init.h
