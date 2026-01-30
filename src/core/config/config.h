/*!****************************************************************************
 * @file  config.h
 * @brief Environement top configuration data
 * This section contain all the configuration data used to setup the RC core environement.
 * These data are shared by all "nodes" (remote, machine ...) to ensure compatibility and coherence.
 * Modififacations will affect all sub projects. Edit it carefully
 * 
 * It must be include in all other sub-config file
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

/**
* Environement variable
*/

#define DISCONECT_DELAY         000       // max delay to sleep devices when a controller not respond
#define DEBOUNCE_MS             100       // debounce delay in ms



/**
 * @brief Internal communication bus configuration 
 * Place here the communication bus configuration such as :
 * - Analog channels configuration
 * - Digital channel configuration
 * 
 * NOTE:
 * - enum entry and analogBusArray/digitalBusArray MUST have the same order
 * - if a entry is not used, set  .analogBus = nullptr  and/or  .srvDev = digitalBus
 * 
 * See Internal command structure definition /include/struct.h for more info
 */

  // analog channels index
enum ComBusAnalogCh { STEERING_BUS = 0,
                      DRIVE_SPEED_BUS,
                      DUMP_BUS,
                      COMBUS_ANALOG_CH_COUNT
                    };

  // analog channels configuration array declaration
extern AnalogBus analogBusArray[];


  // digital channels index
enum ComBusDigitalCh{ HORN = 0,
                      LIGHTS,
                      COMBUS_DIGITAL_CH_COUNT
                    };

  // digital channels configuration array declaration
extern DigitalBus digitalBusArray[];



/**
 * @brief Communication bus structure definition
 */

 extern ComBus comBus;

// EOF config.h