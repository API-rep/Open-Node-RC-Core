/*!****************************************************************************
 * @file  config.h
 * @brief Top configuration file
 * This file contain all the editable data used for the vehicle setup
 * It is used in "top" of all other sub config files to allow user editable variable
 * to be set before compilation
 * NOTE:
 * - Edit ONLY this file, no the sub config files.
 * - Include this file in top of config dependent code. All other sub config file will be
 *   automaticly incude.
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include <core/config/config.h>

/**
 * @brief remote selection.
 * Uncomment one of this line (remove //) to select a vehicle
 *   or
 * specify a -DREMOTE=... parameter in compiler command line.
 */


	// Default runlevel at power-on (see available modes in runlevel.h).
#define DEF_RUNLEVEL    RunLevel::IDLE


#include "machines/machines.h"


// EOF config.h