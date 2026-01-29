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
#include <struct.h>
#include <macro.h>
#include <defs/defs.h>


/**
 * @brief vehicle selection.
 * Uncomment one of this line (remove //) to select a vehicle
 *   or
 * specify a -DMACHINE=... parameter in compiler command line.
 */

#ifndef MACHINE
  #define MACHINE  VOLVO_A60_H_BRUDER
  //#define MACHINE  ANOTHER_MACHINE
#endif

#ifndef DC_DRIVER_MODEL
  #define DC_DRIVER_MODEL  DRV8801
  //  #define DC_DRIVER_MODEL  DRV8874
#endif

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

/**
 * @brief motors/servo settings
 * 
 */

#define M_DEF_PWM_FREQ        16000       // motors default PWM frequency (in hz)
#define SRV_DEF_PWM_FREQ         50       // servo default PWM frequency (in hz)
#define COOLING_FAN_SPEED       100       // cooling fan speed (in %)


#include "../common/sys_config/sys_config.h"
#include "config/machines.h"
#include "config/inputs/inputs.h"

// EOF config.h