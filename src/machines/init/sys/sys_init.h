/*****************************************************************************
 * @file sys_init.h
 * @brief System configuration and environment initialization
 * 
 * This script MUST be included via the init.h umbrella file.
 * ***************************************************************************/
#pragma once

#include <core/system/hw/pin_reg.h>

// =============================================================================
// 1. SINGLETON
// =============================================================================

/// Board-level GPIO registry for the machine environment.
/// Defined in sys_init.cpp — populated during sys_init().
extern PinReg pinReg;


// =============================================================================
// 2. SYSTEM INITIALIZATION
// =============================================================================

	/// System-level init — environment parsing, bus limits, sanity checks.
void sys_init();

// EOF sys_init.h
