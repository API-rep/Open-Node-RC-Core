/*!****************************************************************************
 * @file  dumper_truck_ids.h
 * @brief Dumper truck com-bus channel ID enumerations
 *
 * Contains ONLY the AnalogComBusID / DigitalComBusID enums inside
 * namespace DumperTruck. Zero project dependencies — safe to include
 * from any header without risk of include cycles.
 *
 * Consumers:
 *   - dumper_truck.h   — include these IDs as part of the full layout
 *   - combus_ids.h     — umbrella dispatcher for struct headers
 *
 * On machine builds, combus_ids.h and combus_types.h adds using namespace DumperTruck;
 * So all existing code uses the names unqualified.
 * Remote builds will keep the explicit prefix (DumperTruck::AnalogComBusID::...) to
 * avoid collisions with other machine configs.
 *******************************************************************************///
#pragma once

#include <cstdint>


namespace DumperTruck {

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

}  // namespace DumperTruck

// EOF dumper_truck_ids.h
