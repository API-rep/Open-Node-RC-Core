/*****************************************************************************
 * @file hw_init.h
 * @brief Hardware configuration and device initialization
 *****************************************************************************/
#pragma once

#include <core/config/combus/combus.h>

#include "hw_init_drv.h"
#include "hw_init_srv.h"


// =============================================================================
// 1. HARDWARE SANITY CHECKS
// =============================================================================

/**
 * @brief Verify hardware configuration coherence
 */
void checkHwConfig();

// =============================================================================
// 2. MAIN INITIALIZATION ROUTINE
// =============================================================================

/**
 * @brief Main hardware configuration routine
 */
void machine_hardware_setup();

// =============================================================================
// 3. DEBUG & MONITORING
// =============================================================================

#ifdef DEBUG_HW_INIT
  /**
   * @brief Print hardware configuration for debugging
   */
  void debugVehicleConfig();
  #define LOG_HW_CONFIG() debugVehicleConfig()
#else
  #define LOG_HW_CONFIG()
#endif

// EOF hw_init.h