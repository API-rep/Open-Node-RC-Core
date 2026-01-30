/*!****************************************************************************
 * @file  dumper_truck.h
 * @brief Dumper truck com-bus configuration file
  * Place here the communication bus configuration such as :
 * - Analog channels configuration
 * - Digital channel configuration
 * 
 * NOTE:
 * - enum entry and analogBusArray/digitalBusArray MUST have the same order
 * - if a entry is not used, set  .analogBus = nullptr  and/or  .srvDev = digitalBus
 * 
 * See com-bus structure definition /include/struct/core_struct.h for more info
 *******************************************************************************/// 
#pragma once

#include <cstdint>

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>


  /** @brief Com-bus analog channels */
enum AnalogComCh {
  STEERING_BUS = 0,
  DRIVE_SPEED_BUS,
  DUMP_BUS,
  COMBUS_ANALOG_CH_COUNT
};

  /** @brief analog channels configuration array declaration */
extern AnalogBus analogBusArray[];



  /** @brief Com-bus digital channels */
enum DigitComCh {
  HORN = 0,
  LIGHTS,
  COMBUS_DIGITAL_CH_COUNT
};

  /** @brief digital channels configuration array declaration */
extern DigitalBus digitalBusArray[];



  /** @brief Communication bus structure definition */
extern ComBus comBus;

// EOF dumper_truck.h