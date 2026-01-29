/*!****************************************************************************
 * @file  macro.h
 * @brief marco main file
 * This file contain macros used in the projet
 * If it grow up, create a sub folder macro with sub macro files and include it on 
 * this main file
 *******************************************************************************/// 
#pragma once

#include <arduino.h>

/**
 * @brief Initialisation macros
 * Designed for main code flexibility regarless of hardware configuration
 */

/**
 * @brief Motor driver instanciation
 */

#ifdef DRV_BRK_CHANNELS
  #if DRV_BRK_CHANNELS == 2
      /** break A and B channels in one command */
    static inline void breakAllMotors() {
        digitalWrite(DRV_CH_A_BRK_PIN, HIGH);
        digitalWrite(DRV_CH_B_BRK_PIN, HIGH);
    }
  #endif
#endif


// EOF macro.h