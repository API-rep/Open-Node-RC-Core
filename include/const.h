/*!****************************************************************************
 * @file  const.h
 * @brief Projects constants definition file
 * This file contain the constants definition used to manage the projet
 * These constants enhance main code structure, maintenance and readability
 * Share it with other deveice of the same project (trailer, light module, sound module ...)
 *******************************************************************************/// 
#pragma once

#include <cstdint>
#define PERCENT_MAX           100   // maximum value in %    

/**
 * @brief Input pin state definition 
 */

enum class InputPinMode : uint8_t {
    ACTIVE_LOW       =  0,
    ACTIVE_HIGH      =  1,
    OPEN_DRAIN_MODE  =  2,
    UNDEFINED        =  3,
};

// EOF const.h
