/**
 * @file combus_manager.h
 * @brief ComBus utility and debug hooks
 */

#pragma once

#include <struct/combus_struct.h>
#include <core/utils/debug/debug_core.h>

/**
 * @brief Reset ComBus drive flags according to active input mapping
 * @param bus Reference to the main communication bus structure
 */
void resetComBusDriveFlags(ComBus &bus);


#if DEBUG_THEME_COMBUS_ENABLED
  #define COMBUS_DEBUG_ENABLED 1
#else
  #define COMBUS_DEBUG_ENABLED 0
#endif

#if COMBUS_DEBUG_ENABLED
  void debugComBusSnapshot(ComBus &bus);
  #define LOG_COMBUS_DEBUG(b) debugComBusSnapshot(b)
#else
  #define LOG_COMBUS_DEBUG(b)
#endif

// EOF combus_manager.h
