/*!****************************************************************************
 * @file  inputs_map.h
 * @brief Input device to com-bus structure map top file
 * This file is the umbrella for input device to com-bus mapping structure types 
 * definition. From -D INPUT_DEVICE flag, it includes the correct mapping structure
 * types definition file into the project.
 *******************************************************************************/// 
#pragma once


#if defined(INPUT_PS4_DS4_BT)
  #include "PS4_dualshock_map.h"

// #elif defined(INPUT_ANOTHER_DEVICE)
//   #include "another_input.h"

#elif defined(INPUT_NONE)
  // no input module selected, nothing to map to control

#else
    #error "No input mapping found for this input module. Check input module compatibility and platformio.ini file to fix the problem."
#endif


// EOF inputs_map.h
