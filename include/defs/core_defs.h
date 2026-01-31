/*!****************************************************************************
 * @file  core_defs.h
 * @brief RC system core environement definitions.
 * @details This file defines datas common on all core nodes.
 *******************************************************************************/// 
#pragma once

#include <cstdint>

/**
 * @brief Status flags definition
 */
enum class Status : uint8_t {
  NOT_SET = 0,    ///< Not set state
  SET     = 1     ///< generic dumper truck layout
};


/**
 * @brief Com-bus presets
 */

enum class CombusLayout : uint8_t {
  UNDEFINED     =  0,    ///< No layout assigned
  DUMPER_TRUCK  =  1     ///< generic dumper truck layout
};

// EOF core_defs.h