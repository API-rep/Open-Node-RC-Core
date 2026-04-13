/*!****************************************************************************
 * @file  machine_config.h
 * @brief Machine-type config umbrella — top-level dispatcher.
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

#include <defs/defs.h>   ///< DUMPER_TRUCK and sibling constants


// =============================================================================
// 1. MACHINE-CLASS DISPATCH
// =============================================================================

#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include <core/config/machines/dumper_truck/dumper_truck_config.h>

// #elif defined(MACHINE_TYPE) && (MACHINE_TYPE == WHEEL_LOADER)
//   #include <core/config/machines/wheel_loader/wheel_loader_config.h>

#else
  #error "machine_config.h: MACHINE_TYPE undefined or no dispatch branch — check platformio.ini"
#endif


// EOF machine_config.h
