/*!****************************************************************************
 * @file  inputs_defs.h
 * @brief Input interface and remote control component definitions.
 * @details This file defines the types of user inputs, communication, and 
 *          processing modes for remote or local control signals.
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

enum class RemoteComponent : uint8_t {
  UNDEFINED      =  0,    ///< Component not initialized
  ANALOG_STICK   =  1,    ///< analogic joystick axis
  ANALOG_BUTTON  =  2,    ///< analogic button
  PUSH_BUTTON    =  3     ///< standard push button
};

// EOF inputs_defs.h