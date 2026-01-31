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

#include <core/config.h>

/**
 * @brief remote selection.
 * Uncomment one of this line (remove //) to select a vehicle
 *   or
 * specify a -DREMOTE=... parameter in compiler command line.
 */

#ifndef REMOTE
  #define REMOTE  PS4_DS4_BT
  //#define REMOTE  ANOTHER_REMOTE
#endif

  // addressing
#ifndef PS4_BLUETOOTH_ADDRESS
  #define PS4_BLUETOOTH_ADDRESS "28:3a:4d:14:e6:e7"
#endif


 // default runlevels at power on (see available mode in "runlevel.h")
#define DEF_RUNLEVEL    RunLevel::RUNNING

  // main battery voltage monitoring
// #define VBAT_SENSING
  #ifdef VBAT_SENSING
    #define MIN_VBAT_SENSE           1.2     // voltage on BAT_SENSE_PIN for 9.5V
    #define VBAT_SENSE_INTERVAL      100     // delay between VBAT sensing in ms
    #define VBAT_SENSE_SAMPLING       10     // number of VBAT measure used to compute average value
  #endif


#include "machines/machines.h"


// EOF config.h