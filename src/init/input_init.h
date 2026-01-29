/*!****************************************************************************
 * @file input_init.h
 * @brief Input system initialisation script
 * This section contain scripts an routines used to initialize the input system.
 * By this it :
 * - Parse remote config file to rich C code (const, var, struct)
 * - Parse input config file to rich C code (const, var, struct)
 * - Do some sanitary check
 * - Compute hardware related environement value
 * - Create harware device object
 * 
 * This script MUST be include in init.h top file
 *******************************************************************************/// 
#pragma once

#include "config.h"

#include <struct.h>
#include <const.h>
#include <macro.h>

/**  ### INPUT DEVICE CONFIGURATION ### */

/**
 * @brief Input devices configuration
 * internal input system structure initialisation
 * - Build coherent naming access to analog/digital channel config structure 
 * - Instance analog channel/digital devices structure
 * - Execute some sanitary checks if need
 */

  // 1. Build coherent naming access to board DC driver device structure 

  // Parse analog channel config
enum {
  #ifdef REMOTE_A_CH_1
    ANALOG_CH_INPUT_1,
  #endif
  #ifdef REMOTE_A_CH_2
    ANALOG_CH_INPUT_2,
  #endif
  #ifdef REMOTE_A_CH_3
    ANALOG_CH_INPUT_3,
  #endif
  #ifdef REMOTE_A_CH_4
    ANALOG_CH_INPUT_4,
  #endif
  #ifdef REMOTE_A_CH_5
    ANALOG_CH_INPUT_5,
  #endif
  #ifdef REMOTE_A_CH_6
    ANALOG_CH_INPUT_6,
  #endif
  #ifdef REMOTE_A_CH_7
    ANALOG_CH_INPUT_7,
  #endif
  #ifdef REMOTE_A_CH_8
    ANALOG_CH_INPUT_8,
  #endif
  #ifdef REMOTE_A_CH_9
    ANALOG_CH_INPUT_9,
  #endif
  #ifdef REMOTE_A_CH_10
    ANALOG_CH_INPUT_10,
  #endif
  #ifdef REMOTE_A_CH_11
    ANALOG_CH_INPUT_11,
  #endif
  #ifdef REMOTE_A_CH_12
    ANALOG_CH_INPUT_12,
  #endif
  #ifdef REMOTE_A_CH_13
    ANALOG_CH_INPUT_13,
  #endif
  #ifdef REMOTE_A_CH_14
    ANALOG_CH_INPUT_14,
  #endif
  #ifdef REMOTE_A_CH_15
    ANALOG_CH_INPUT_15,
  #endif
  #ifdef REMOTE_A_CH_16
    ANALOG_CH_INPUT_16,
  #endif
      // return total number of defined analog channels
    NUM_OF_ANALOG_CH_INPUT
};

  // Parse digital channel config
enum {
  #ifdef REMOTE_D_CH_1
    DIGIT_CH_INPUT_1,
  #endif
  #ifdef REMOTE_D_CH_2
    DIGIT_CH_INPUT_2,
  #endif
  #ifdef REMOTE_D_CH_3
    DIGIT_CH_INPUT_3,
  #endif
  #ifdef REMOTE_D_CH_4
    DIGIT_CH_INPUT_4,
  #endif
  #ifdef REMOTE_D_CH_5
    DIGIT_CH_INPUT_5,
  #endif
  #ifdef REMOTE_D_CH_6
    DIGIT_CH_INPUT_6,
  #endif
  #ifdef REMOTE_D_CH_7
    DIGIT_CH_INPUT_7,
  #endif
  #ifdef REMOTE_D_CH_8
    DIGIT_CH_INPUT_8,
  #endif
  #ifdef REMOTE_D_CH_9
    DIGIT_CH_INPUT_9,
  #endif
  #ifdef REMOTE_D_CH_10
    DIGIT_CH_INPUT_10,
  #endif
  #ifdef REMOTE_D_CH_11
    DIGIT_CH_INPUT_11,
  #endif
  #ifdef REMOTE_D_CH_12
    DIGIT_CH_INPUT_12,
  #endif
  #ifdef REMOTE_D_CH_13
    DIGIT_CH_INPUT_13,
  #endif
  #ifdef REMOTE_D_CH_14
    DIGIT_CH_INPUT_14,
  #endif
  #ifdef REMOTE_D_CH_15
    DIGIT_CH_INPUT_15,
  #endif
  #ifdef REMOTE_D_CH_16
    DIGIT_CH_INPUT_16,
  #endif
      // return total number of defined digital channels
    NUM_OF_DIGIT_CH_INPUT
};


  // 2. Instance analog channel/digital devices structure

  // 2.1. AnalogDev[] array filling macro
#if  NUM_OF_ANALOG_CH_INPUT > 0
  #define FILL_A_DEV(n) { \
      .infoName   = REMOTE_A_CH_##n##_INFO_NAME, \
      .type       = REMOTE_A_CH_##n##_DEV_TYPE,  \
      .minVal     = REMOTE_A_CH_##n##_MIN_VAL,   \
      .maxVal     = REMOTE_A_CH_##n##_MAX_VAL,   \
      .isInverted = REMOTE_A_CH_##n##_INVERTED   \
  }
  
    // Temp AnalogDev[] array creation
  inline AnalogDev _tmpAnalogDev[] = {
  #ifdef REMOTE_A_CH_1
    FILL_A_DEV(1),
  #endif

  #ifdef REMOTE_A_CH_2
    FILL_A_DEV(2),
  #endif

  #ifdef REMOTE_A_CH_3
    FILL_A_DEV(3),
  #endif

  #ifdef REMOTE_A_CH_4
    FILL_A_DEV(4),
  #endif

  #ifdef REMOTE_A_CH_5
    FILL_A_DEV(5),
  #endif

  #ifdef REMOTE_A_CH_6
    FILL_A_DEV(6),
  #endif

  #ifdef REMOTE_A_CH_7
    FILL_A_DEV(7),
  #endif

  #ifdef REMOTE_A_CH_8
    FILL_A_DEV(8),
  #endif

  #ifdef REMOTE_A_CH_9
    FILL_A_DEV(9),
  #endif

  #ifdef REMOTE_A_CH_10
    FILL_A_DEV(10),
  #endif

  #ifdef REMOTE_A_CH_11
    FILL_A_DEV(11),
  #endif

  #ifdef REMOTE_A_CH_12
    FILL_A_DEV(12),
  #endif

  #ifdef REMOTE_A_CH_13
    FILL_A_DEV(13),
  #endif

  #ifdef REMOTE_A_CH_14
    FILL_A_DEV(14),
  #endif

  #ifdef REMOTE_A_CH_15
    FILL_A_DEV(15),
  #endif

  #ifdef REMOTE_A_CH_16
    FILL_A_DEV(16),
  #endif    
  };
#else
    // Init null pointer to AnalogDev[] array
  inline AnalogDev* _tmpAnalogDev = nullptr; 
#endif


  // 2.2. DigitalDev[] array filling macro
#if  NUM_OF_DIGIT_CH_INPUT > 0
  #define FILL_D_DEV(n) { \
      .infoName   = REMOTE_D_CH_##n##_INFO_NAME, \
      .type       = REMOTE_D_CH_##n##_DEV_TYPE,  \
      .isInverted = REMOTE_D_CH_##n##_INVERTED   \
  }
  
    // Temp DigitalDev[] array creation
  inline DigitalDev _tmpDigitalDev[] = {
  #ifdef REMOTE_D_CH_1
    FILL_D_DEV(1),
  #endif

  #ifdef REMOTE_D_CH_2
    FILL_D_DEV(2),
  #endif

  #ifdef REMOTE_D_CH_3
    FILL_D_DEV(3),
  #endif
  
  #ifdef REMOTE_D_CH_4
    FILL_D_DEV(4),
  #endif
  
  #ifdef REMOTE_D_CH_5
    FILL_D_DEV(5),
  #endif
  
  #ifdef REMOTE_D_CH_6
    FILL_D_DEV(6),
  #endif
  
  #ifdef REMOTE_D_CH_7
    FILL_D_DEV(7),
  #endif
  
  #ifdef REMOTE_D_CH_8
    FILL_D_DEV(8),
  #endif
  
  #ifdef REMOTE_D_CH_9
    FILL_D_DEV(9),
  #endif
  
  #ifdef REMOTE_D_CH_10
    FILL_D_DEV(10),
  #endif
  
  #ifdef REMOTE_D_CH_11
    FILL_D_DEV(11),
  #endif
  
  #ifdef REMOTE_D_CH_12
    FILL_D_DEV(12),
  #endif
  
  #ifdef REMOTE_D_CH_13
    FILL_D_DEV(13),
  #endif
  
  #ifdef REMOTE_D_CH_14
    FILL_D_DEV(14),
  #endif
  
  #ifdef REMOTE_D_CH_15
    FILL_D_DEV(15),
  #endif
  
  #ifdef REMOTE_D_CH_16
    FILL_D_DEV(16),
  #endif
};
#else
    // Init null pointer to DigitalDev[] array
  inline DigitalDev* _tmpDigitalDev = nullptr; 
#endif

  // final input devices structure creation
inline InputDev inputDev = {
  .status       = NOT_SET,
  .analogDev    = _tmpAnalogDev,
  .digitalDev   = _tmpDigitalDev,
  .numAnalogCh  = (uint8_t)NUM_OF_ANALOG_CH_INPUT,
  .numDigitalCh = (uint8_t)NUM_OF_DIGIT_CH_INPUT
};

  // 3. Check input device analog/digital capacity
 static_assert(NUM_OF_ANALOG_CH_INPUT <= 16, "Too many analog channels for the communication bus.");
 static_assert(NUM_OF_DIGIT_CH_INPUT <= 16, "Too many digital channels for the communication bus.");

// EOF input_init.h