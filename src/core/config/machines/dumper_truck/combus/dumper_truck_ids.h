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
 *   - combus_ids.h     — umbrella dispatcher for struct headers (src/core/config/machines/combus_ids.h)
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
  ENGINE_RPM_BUS,      ///< Raw throttle stick value — drives engine sound RPM (owner: INPUT_DEV)
  DUMP_BUS,
  ESC_SPEED_BUS,       ///< Inertia-smoothed speed — drives ESC output + currentSpeed (owner: SYSTEM or SYSTEM_EXT)
  CH_COUNT
};

/// @brief Com-bus digital channel identifiers
enum class DigitalComBusID : uint8_t {
  HORN = 0,
  LIGHTS,
  KEY,              ///< Ignition key. Set to start/rearm the machine.
  BATTERY_LOW,      ///< Battery low flag — written by vbat module, read by all receivers.
  INDICATOR_LEFT,   ///< Left turn indicator — set by machine from operator input.
  INDICATOR_RIGHT,  ///< Right turn indicator — set by machine from operator input.
  HAZARDS,          ///< Hazard lights (both indicators) — set by machine from operator input.
  CH_COUNT
};

}  // namespace DumperTruck

// EOF dumper_truck_ids.h
