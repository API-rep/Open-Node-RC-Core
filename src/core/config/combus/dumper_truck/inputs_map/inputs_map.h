/*!****************************************************************************
 * @file  inputs_map.h
 * @brief Input device to com-bus structure map top file
 * This file is the umbrella for input device to com-bus mapping structure types 
 * definition. From -D INPUT_DEVICE flag, it includes the correct mapping structure
 * types definition file into the project.
 *******************************************************************************/// 
#pragma once

/** @brief Aiguillage automatique du mapping selon l'INPUT_MODULE */
#if INPUT_MODULE == PS4_DS4_BT
  #include "PS4_dualshock_map.h"

#elif INPUT_MODULE == ANOTHER_INPUT_DEVICE
  #include "another_input_map.h"

#else
    #error "No input mapping found for this input module. Check input module compatibility and platformio.ini file to fix the problem."
#endif


// EOF inputs_map.h