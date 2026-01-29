/*!****************************************************************************
 * @file sys_init.h
 * @brief System config initialisation script
 * This section contain scripts an routines used to parse system config file and create
 * the environement data structure.
 * By this it :
 * - Parse config file to rich C code (const, var, struct)
 * - Do some sanitary check
 * - Compute system related environement value
 * - Create harware device object
 * 
 * This script MUST be include in init.h top file
 *******************************************************************************/// 
#pragma once

#include <struct.h>
#include <const.h>
#include <macro.h>

#include <config/config.h>


  // system configuration function
void sys_init_setup();

// EOF sys_init.h