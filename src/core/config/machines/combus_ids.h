/*!****************************************************************************
 * @file  combus_ids.h
 * @brief Com-bus channel ID dispatcher — the single #if chain for IDs
 *
 * Dispatches to the active machine's *_ids.h (enum IDs only, zero deps).
 * Adds `using namespace <EnvCfg>` so machine-side code uses unqualified names.
 *
 * WHY THIS FILE EXISTS:
 *   struct headers (machines_struct.h, remotes_map_struct.h) need
 *   AnalogComBusID / DigitalComBusID as field types but cannot include
 *   combus_types.h (would create an include cycle with combus_struct.h).
 *   Including combus_ids.h breaks the cycle because *_ids.h files have
 *   only <cstdint> as a dependency.
 *
 * HOW TO ADD A NEW MACHINE:
 *   1. Create src/core/config/machines/<name>/combus/<name>_ids.h with the two enums
 *      inside  namespace <Name> { }
 *   2. Add ONE #elif branch below (include + using namespace).
 *   That is the only file to touch — no changes needed in struct headers.
 *******************************************************************************///
#pragma once

#include <const.h>


#if MACHINE == VOLVO_A60_H_BRUDER
  #include <core/config/machines/dumper_truck/combus/combus_ids.h>
  using namespace DumperTruck;

// #elif MACHINE == FUTURE_EXCAVATOR_VEHICLE     // TODO winter 2026
//   #include <core/config/machines/excavator/combus/excavator_ids.h>
//   using namespace Excavator;

// #elif MACHINE == FUTURE_LOADER_VEHICLE        // TODO winter 2026
//   #include <core/config/machines/loader/combus/loader_ids.h>
//   using namespace WheelLoader;

#else
  #error "combus_ids.h: MACHINE undefined or unsupported — add a -D MACHINE= build flag."
#endif


// EOF combus_ids.h
