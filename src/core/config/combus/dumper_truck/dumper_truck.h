/*!****************************************************************************
 * @file  dumper_truck.h
 * @brief Dumper truck com-bus configuration file
  * Place here the communication bus configuration such as :
 * - Analog channels configuration
 * - Digital channel configuration
 * 
 * NOTE:
 * - enum entry and AnalogComBusArray/DigitalComBusArray MUST have the same order
 * - if a entry is not used, set  .analogBus = nullptr  and/or  .srvDev = digitalBus
 * 
 * See com-bus structure definition /include/struct/core_struct.h for more info
 *******************************************************************************/// 
#pragma once

#include <cstdint>

#include <struct/combus_struct.h>
#include "dumper_truck_types.h"


  /** @brief Com-bus analog channels configuration array */
extern AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)];


  /** @brief Com-bus digital channels configuration array */
extern DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)];


  /** @brief Communication bus structure definition */
extern ComBus comBus;


  // include automatically the correct input mapping file according for this combus type
#include "inputs_map/inputs_map.h"

// EOF dumper_truck.h