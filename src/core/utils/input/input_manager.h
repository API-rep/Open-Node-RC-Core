/**
 * @file input_manager.h
 * @brief Input management module for Open-Node-RC-Core
 */

#pragma once

#include <struct/combus_struct.h>
#include <core/utils/debug/debug.h>

/** @brief Initialize the selected input module */

void input_setup();



/**
 * @brief Sync physical input device data with the internal ComBus
 * @param bus Reference to the main communication bus structure
 */

void input_update(ComBus &bus);

/**
 * @brief Return the configured input device name (from input config infoName).
 */
const char* input_get_name();


// EOF input_manager.h
