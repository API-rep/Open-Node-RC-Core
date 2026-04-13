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

	// Machine selector and bus enums
#include <core/config/machines/combus_types.h>

// =============================================================================
// 3. REMOTE MAPPING STRUCTURES
// =============================================================================

	// Input mapping definitions
#include <struct/remotes_map_struct.h>


// =============================================================================
// 4. INPUT INITIALIZATION
// =============================================================================

	/// Input system init — input_setup() wrapper with init-sequence log output.
void input_init();

// EOF input_init.h