/**
 * @file combus_manager.h
 * @brief ComBus utility and debug hooks
 */

#pragma once

#include <struct/combus_struct.h>
#include <core/utils/debug/debug.h>

/**
 * @brief Reset ComBus drive flags according to active input mapping
 * @param bus Reference to the main communication bus structure
 */
void resetComBusDriveFlags(ComBus &bus);


#if defined(DEBUG_COMBUS) || defined(DEBUG_ALL)
  void debugComBusSnapshot(ComBus &bus);
  #define LOG_COMBUS_DEBUG(b) debugComBusSnapshot(b)
#else
  #define LOG_COMBUS_DEBUG(b)
#endif

// EOF combus_manager.h
