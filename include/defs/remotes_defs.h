/*!****************************************************************************
 * @file  remotes_defs.h
 * @brief Remotes devices definitions.
 * @details This file defines all data related to remote devive.
 *******************************************************************************/// 

#pragma once

#include <cstdint>

/**
 * @brief Remote physical component types
 */

enum class RemoteComponent : uint8_t {
  UNDEFINED      =  0,    ///< Component not initialized
  ANALOG_STICK   =  1,    ///< analogic joystick axis
  ANALOG_BUTTON  =  2,    ///< analogic button
  PUSH_BUTTON    =  3     ///< standard push button
};

// EOF remotes_defs.h