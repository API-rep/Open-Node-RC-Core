/*!****************************************************************************
 * @file  const.h
 * @brief Projects constants definition file
 * This file contain the constants definition used to manage the projet
 * These constants enhance main code structure, maintenance and readability
 * Share it with other deveice of the same project (trailer, light module, sound module ...)
 *******************************************************************************/// 
#pragma once

#include <cstdint>

#define NOT_SET                -1
#define PERCENT_MAX           100   // maximum value in %    

/**
 * @brief Input pin state definition 
 */

enum class InputPinMode : uint8_t {
    UNDEFINED        =  0,
    ACTIVE_LOW       =  1,
    ACTIVE_HIGH      =  2,
    OPEN_DRAIN_MODE  =  3,
};

// EOF const.h
