/*!****************************************************************************
 * @file  dumper_truck.h
 * @brief Dumper truck com-bus — channel IDs, externs and input mapping
 *
 * This single file covers three scopes:
 *
 *   Always included (all build environments):
 *     - AnalogComBusID / DigitalComBusID enums
 *
 *   Single-combus nodes (all except IS_REMOTE):
 *     - AnalogComBusArray / DigitalComBusArray / comBus externs
 *
 *   Nodes with an input module (INPUT_MODULE defined):
 *     - inputs_map.h (input-device → com-bus mapping)
 *
 * NOTE:
 * - enum entry and AnalogComBusArray/DigitalComBusArray MUST have the same order
 * - if an entry is not used, set  .analogBus = nullptr  and/or  .srvDev = digitalBus
 *
 * See com-bus structure definition /include/struct/combus_struct.h for more info
 *******************************************************************************///
#pragma once

// =============================================================================
// 1. COM-BUS CHANNEL IDs  (available in all build environments)
// =============================================================================

#include "dumper_truck_ids.h"   ///< AnalogComBusID / DigitalComBusID inside namespace DumperTruck


// =============================================================================
// 2. COM-BUS EXTERNS  (single-combus nodes — excluded for IS_REMOTE)
// =============================================================================

#ifndef IS_REMOTE

#include <struct/combus_struct.h>

/// @brief Com-bus analog channels configuration array
extern AnalogComBus  AnalogComBusArray[static_cast<uint8_t>(DumperTruck::AnalogComBusID::CH_COUNT)];

/// @brief Com-bus digital channels configuration array
extern DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DumperTruck::DigitalComBusID::CH_COUNT)];

/// @brief Communication bus structure
extern ComBus comBus;

#endif  // !IS_REMOTE


// =============================================================================
// 3. INPUT MAPPING  (nodes with an input device module)
// =============================================================================

#ifdef INPUT_MODULE
#include "inputs_map/inputs_map.h"
#endif  // INPUT_MODULE

// EOF dumper_truck.h