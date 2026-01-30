/*!****************************************************************************
 * @file hw_init.h
 * @brief Hardware config initialisation script
 * This section contain scripts an routines used to parse harware config file and
 * create the vehicle environement data.
 * By this it :
 * - Do some sanitary check of config file and hardware capability
 * - Create harware device object and initalize them with config file value
 * - Print debug msg if debug flags are set
 * 
* This script MUST be include in init.h top file
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include "hw_init_drv.h"
#include "hw_init_srv.h"

/**
 * @brief Check harware config file
 * Verify hardware config file coherence.
 */

  // Check motherboard DC driver capacity -> chage to serial print with boardCfg.dcDrvCount value
//(((static_assert (mCfg.dcDrvCount <=  MAX_ONBOARD_DC_DRV, "Too much DC driver configured for the motherboard. Check board.h and machines.h to fix the problem."); )))
  // check DC driver config file coherence
void checkHwConfig();



/**
 * @brief Main harware configuration routine
 * Create harware device object and initalize them with config file value.
 * Call this function once in setup() main.cpp file
 */

void machine_hardware_setup();



/**
 * @brief Debug harware configuration
 * Print debug msg if debug flags are set
 */

void debugVehicleConfig();

// EOF hw_init.h