/**
 * @file input_manager.h
 * @brief Input management module for Open-Node-RC-Core
 */

#pragma once

#include <struct/combus_struct.h>

/** @brief Initialize the selected input module */

void input_setup();



/**
 * @brief Sync physical input device data with the internal ComBus
 * @param bus Reference to the main communication bus structure
 */

void input_update(ComBus &bus);



/**
 * @brief Verbose debug output for input mapping
 * @param bus Reference to the main communication bus structure
 */

#ifdef DEBUG_INPUT
  void debugInputMapping(ComBus &bus);
  #define LOG_INPUT_DEBUG(b) debugInputMapping(b)

#else
  #define LOG_INPUT_DEBUG(b) 
#endif