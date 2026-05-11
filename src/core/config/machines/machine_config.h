/*!****************************************************************************
 * @file  machine_config.h
 * @brief EnvCfg-type config umbrella — top-level dispatcher.
 *
 * @details Dispatches to the machine-class sub-umbrella based on MACHINE_TYPE.
 *          Including this single header brings in all active sub-modules for
 *          the current build target (combus, motion, inputs_map, sound …).
 *          Each sub-umbrella controls which sub-modules are compiled in via
 *          build flags (SOUND_NODE, INPUT_MODULE, etc.).
 *
 *          To add a new machine class:
 *          1. Create src/core/config/machines/<class>/<class>_config.h
 *          2. Add one #elif branch below.
 *          No other file needs to change.
 *******************************************************************************
 */
#pragma once

#include <defs/defs.h>


// =============================================================================
// 1. MACHINE DISPATCH
// =============================================================================

#if MACHINE == VOLVO_A60_H_BRUDER
  #include <core/config/machines/dumper_truck/dumper_truck_config.h>

// #elif MACHINE == FUTURE_EXCAVATOR_VEHICLE     // TODO winter 2026
//   #include <core/config/machines/excavator/excavator_config.h>

// #elif MACHINE == FUTURE_LOADER_VEHICLE        // TODO winter 2026
//   #include <core/config/machines/loader/loader_config.h>

#else
  #error "machine_config.h: MACHINE undefined or unsupported — add a -D MACHINE= build flag"
#endif


// EOF machine_config.h
