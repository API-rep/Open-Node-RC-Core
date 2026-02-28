/*****************************************************************************
 * @file hw_init.h
 * @brief Hardware configuration and device initialization
 *****************************************************************************/
#pragma once

#include <core/config/combus/combus.h>
#include <core/utils/debug/debug.h>

#include "hw_init_drv.h"
#include "hw_init_srv.h"


// =============================================================================
// 1. MAIN INITIALIZATION ROUTINE
// =============================================================================

	/// Hardware initialization — config check then driver and servo setup.
void hw_init();

// =============================================================================
// 2. DEBUG & MONITORING
// =============================================================================

#if defined(DEBUG_HW) || defined(DEBUG_ALL)
    /// Print machine hardware configuration for debugging.
  void debugVehicleConfig();
  #define LOG_HW_CONFIG() debugVehicleConfig()
#else
  #define LOG_HW_CONFIG()
#endif

// EOF hw_init.h