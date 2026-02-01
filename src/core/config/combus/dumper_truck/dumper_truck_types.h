/*!****************************************************************************
 * @file  dumper_truck_types.h
 * @brief Dumper truck com-bus digital/analog types ID definition file
 * Place here the identifier for analog and digital com-bus channels. 
 * 
 * NOTE:
 * - enum entry and AnalogComBusArray/DigitalComBusArray entry in associate file
 * MUST have the SAME order and the SAME number of entry.
 *******************************************************************************/// 
#pragma once

#include <cstdint>

  /** @brief Com-bus analog channels ID*/
enum class AnalogComBusID : uint8_t {
  STEERING_BUS = 0,
  DRIVE_SPEED_BUS,
  DUMP_BUS,
  CH_COUNT
};



  /** @brief Com-bus digital channels ID*/
enum class DigitalComBusID : uint8_t {
  HORN = 0,
  LIGHTS,
  CH_COUNT
};

// EOF dumper_truck_types.h