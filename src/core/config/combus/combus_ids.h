/*!****************************************************************************
 * @file  combus_ids.h
 * @brief Com-bus channel ID dispatcher — the single #if chain for IDs
 *
 * Dispatches to the active machine's *_ids.h (enum IDs only, zero deps).
 * Adds `using namespace <Machine>` so machine-side code uses unqualified names.
 *
 * WHY THIS FILE EXISTS:
 *   struct headers (machines_struct.h, remotes_map_struct.h) need
 *   AnalogComBusID / DigitalComBusID as field types but cannot include
 *   combus_types.h (would create an include cycle with combus_struct.h).
 *   Including combus_ids.h breaks the cycle because *_ids.h files have
 *   only <cstdint> as a dependency.
 *
 * HOW TO ADD A NEW MACHINE:
 *   1. Create src/core/config/combus/<name>/<name>_ids.h with the two enums
 *      inside  namespace <Name> { }
 *   2. Add ONE #elif branch below (include + using namespace).
 *   That is the only file to touch — no changes needed in struct headers.
 *******************************************************************************///
#pragma once

#include <const.h>


#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include "dumper_truck/dumper_truck_ids.h"
  using namespace DumperTruck;

// #elif defined(MACHINE_TYPE) && (MACHINE_TYPE == AUTRE_MACHINE_EXEMPLE)
//   #include "autre_machine/autre_machine_ids.h"
//   using namespace AutreMachine;

#else
  #error "combus_ids.h: unknown MACHINE_TYPE — add a new #elif branch for this machine."
#endif


// EOF combus_ids.h
