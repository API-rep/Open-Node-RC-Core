/*!****************************************************************************
 * @file  remotes_defs.h
 * @brief Remotes devices definitions.
 * @details This file defines all data related to remote devive.
 *******************************************************************************/// 

#pragma once

#include <cstdint>

/**
 * @brief Remote protocol definition 
 */

enum class RemoteProtocol : uint8_t {
  UNDEFINED      =  0,    ///< No protocol assigned
  PS4_BLUETOOTH  =  1     ///< Sony DualShock 4 via BT Stack
};



/**
 * @brief Remote physical component types
 */

enum class RemoteComp : uint8_t {
  UNDEFINED      =  0,    ///< Component not initialized
  ANALOG_STICK   =  1,    ///< analogic joystick axis
  ANALOG_BUTTON  =  2,    ///< analogic button
  PUSH_BUTTON    =  3     ///< standard push button
};

// EOF remotes_defs.h