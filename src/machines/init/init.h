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
#include "sys_init.h"

	// --- Machine hardware setup ---
#include "hw_init.h"

	// --- Input device setup ---
#include "input_init.h"

// EOF init.h