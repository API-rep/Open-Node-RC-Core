/**
 * @file combus_manager.h
 * @brief ComBus utility and debug hooks
 */

#pragma once

#include <struct/combus_struct.h>
#include <core/system/debug/debug.h>

/**
 * @brief Reset ComBus drive flags according to active input mapping
 * @param bus Reference to the main communication bus structure
 */
void resetComBusDriveFlags(ComBus &bus);


// EOF combus_manager.h
