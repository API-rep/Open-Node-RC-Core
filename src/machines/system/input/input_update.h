/**
 * @file input_update.h
 * @brief Machine-side input device -> ComBus mapping
 *
 * @details Consumes the input device value tables refreshed by the core
 *   (core/system/input/input_manager.h) and applies this machine's own
 *   mapping (see .../inputs_map/inputs_map.h) onto the ComBus. This is where
 *   InputAnalogMap/InputDigitalMap, AnalogComBusID/DigitalComBusID and any
 *   machine-specific channel knowledge legitimately live.
 */

#pragma once

#include <core/system/combus/combus_defs.h>   // ComBus

/**
 * @brief Map current input device values onto this machine's ComBus channels.
 * @param bus Reference to the main communication bus structure
 * @note Call input_refresh() (core) once per tick before this function.
 */
void input_update(ComBus &bus);

// EOF input_update.h