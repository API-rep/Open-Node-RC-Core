/*!****************************************************************************
 * @file  sys_defs.h
 * @brief Global system states definitions
 * @details This file centralizes enumerations related to the application lifecycle 
 *          (RunLevels) and system health status.
 *******************************************************************************/// 

#pragma once

#include <cstdint>

  // available run levels
enum class RunLevel : int8_t {
    NOT_YET_SET     = -1,
    IDLE        =  0,
    STARTING    =  1,
    RUNNING     =  2,
    TURNING_OFF =  3,
    SLEEPING    =  4,
    RESET       =  5
};

// EOF sys_defs.h