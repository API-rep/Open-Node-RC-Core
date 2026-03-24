/*****************************************************************************
 * @file init.h
 * @brief Global initialisation sequence for Open-Node-RC-Core
 * 
 * This file orchestrates the loading of constants, structures, and 
 * specific initialization modules (System, Hardware, Inputs).
 * It MUST be included at the very top of main.cpp.
 * 
 * ***************************************************************************/
#pragma once

// =============================================================================
// 1. CORE DEFINITIONS & STRUCTURES
// =============================================================================

	// --- Global constants and bus structures ---
#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

// =============================================================================
// 2. CONFIGURATION & SELECTION
// =============================================================================

	// --- Machine and remote selectors ---
#include <config/config.h>

// =============================================================================
// 3. INITIALIZATION SUB-MODULES
// =============================================================================

	// --- System level setup ---
#include "sys/sys_init.h"

	// --- Machine hardware setup ---
#include "hw/hw_init.h"

	// --- Input device setup ---
#include "input/input_init.h"

	// --- Output peripherals setup ---
#include "output/output_init.h"


// =============================================================================
// 4. INIT ORCHESTRATOR
// =============================================================================

	/// Full initialization sequence — single entry point from setup().
void machine_init();

// EOF init.h