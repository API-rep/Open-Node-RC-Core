/*!****************************************************************************
 * @file  machine_type_combus_ids.h
 * @brief Dispatch umbrella — selects the Remote ComBus vocabulary by vehicle type.
 *
 * @details This is the ONLY file in the project that contains the dispatch
 *   logic for the Remote vocabulary (AnalogComBusRemoteID / DigitalComBusRemoteID).
 *   No other file must duplicate this #if/#elif logic.
 *
 *   The selected header provides the Remote channel IDs for the active machine
 *   type.  These IDs are used by:
 *     - Remote node builds (src/remotes/)
 *     - Sound node builds (src/sound_module/) — ComBus transport contract
 *     - Any generic code that needs to reference REMOTE-only channels
 *
 *   For INSTANCE-specific code (full vocabulary including LOCAL+SYSTEM channels),
 *   include the instance's combus_ids.h directly.
 *******************************************************************************
 */
#pragma once

#include <machines/config/machines/machines.h> // MACHINE_TYPE_*


#if defined(MACHINE_TYPE_DUMPER_TRUCK)
  #include <core/config/machines/dumper_truck/combus_ids_remote.h>

// #elif defined(MACHINE_TYPE_EXCAVATOR)
//   #include <core/config/machines/excavator/combus_ids_remote.h>

// #elif defined(MACHINE_TYPE_WHEEL_LOADER)
//   #include <core/config/machines/wheel_loader/combus_ids_remote.h>

#else
  #error "Unsupported/missing MACHINE_TYPE_* value. Check MACHINE_* config file to fix the problem"

#endif  // MACHINE_TYPE_DUMPER_TRUCK

// EOF machine_type_combus_ids.h