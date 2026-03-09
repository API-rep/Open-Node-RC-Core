/*!****************************************************************************
 * @file  dumper_truck.h
 * @brief Dumper truck com-bus — channel IDs, externs and input mapping
 *
 * This single file covers two scopes controlled by IS_MACHINE:
 *
 *   Always included (machine node + sound node):
 *     - AnalogComBusID / DigitalComBusID enums
 *
 *   IS_MACHINE only (machine node build):
 *     - AnalogComBusArray / DigitalComBusArray / comBus externs
 *     - inputs_map.h (input-device → com-bus mapping)
 *
 * NOTE:
 * - enum entry and AnalogComBusArray/DigitalComBusArray MUST have the same order
 * - if an entry is not used, set  .analogBus = nullptr  and/or  .srvDev = digitalBus
 *
 * See com-bus structure definition /include/struct/combus_struct.h for more info
 *******************************************************************************///
#pragma once

#include <cstdint>


// =============================================================================
// 1. COM-BUS CHANNEL IDs  (available in all build environments)
// =============================================================================

/// @brief Com-bus analog channel identifiers
enum class AnalogComBusID : uint8_t {
  STEERING_BUS = 0,
  DRIVE_SPEED_BUS,
  DUMP_BUS,
  CH_COUNT
};

/// @brief Com-bus digital channel identifiers
enum class DigitalComBusID : uint8_t {
  HORN = 0,
  LIGHTS,
  KEY,          ///< Ignition key. Set to start/rearm the machine.
  BATTERY_LOW,  ///< Battery low flag — written by vbat module, read by all receivers.
  CH_COUNT
};


// =============================================================================
// 2. COM-BUS EXTERNS AND INPUT MAPPING  (machine node only)
// =============================================================================

// IS_MACHINE is the compile-time discriminator between a machine node and a
// cross-boundary consumer (e.g. sound node). It is defined via -D IS_MACHINE
// in platformio.ini for every [env:machines]-derived environment.
// Do NOT remove or rename this flag — the split between sections 1 and 2 depends on it.
#ifdef IS_MACHINE

#include <struct/combus_struct.h>

/// @brief Com-bus analog channels configuration array
extern AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)];

/// @brief Com-bus digital channels configuration array
extern DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)];

/// @brief Communication bus structure
extern ComBus comBus;

  // Automatically includes the correct input mapping for this com-bus type
#include "inputs_map/inputs_map.h"

#endif  // IS_MACHINE

// EOF dumper_truck.h