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


// EOF input_manager.h
