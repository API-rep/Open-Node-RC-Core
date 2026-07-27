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


#if MACHINE_TYPE == DUMPER_TRUCK
  #include <core/config/machines/dumper_truck/combus_ids_remote.h>

// #elif MACHINE_TYPE == EXCAVATOR
//   #include <core/config/machines/excavator/combus_ids_remote.h>
// #elif MACHINE_TYPE == WHEEL_LOADER
//   #include <core/config/machines/wheel_loader/combus_ids_remote.h>
#else
  #error "machine_type_combus_ids.h: MACHINE_TYPE undefined or unsupported. Set MACHINE_TYPE to a known vehicle type (e.g. DUMPER_TRUCK) before including this file."
#endif


// EOF machine_type_combus_ids.h