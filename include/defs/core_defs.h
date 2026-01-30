/*!****************************************************************************
 * @file  core_defs.h
 * @brief RC system core environement definitions.
 * @details This file defines datas common on all core nodes.
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

// EOF core_defs.h