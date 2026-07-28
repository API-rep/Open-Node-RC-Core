/*!****************************************************************************
 * @file  inputs.h
 * @brief Global input module configuration file.
 *
 * @details This file is the single entry point for the input subsystem.
 *   It dispatches to the active input-device module (nop, PS4, RC, etc.)
 *   via -D INPUT_* compile flag and exposes the shared composition structures
 *   (InputAnalogMap, InputDigitalMap) that combine the device vocabulary
 *   with the ComBus channel vocabulary.
 *
 *   Consumers shall include this file only — no direct inclusion of
 *   device-specific headers (PS4_dualshock.h, nop.h) or mapping structs
 *   (remotes_map_struct.h) from outside the input subsystem.
 *
 *   This file MUST be included via config.h or main.cpp.
 *******************************************************************************/
#pragma once

// =============================================================================
// INPUT MODULE DISPATCH
// =============================================================================
//
// Each backend opts in via its own dedicated flag (INPUT_<NAME>), checked
// with #ifdef only — never #if == against an unregistered constant. Backends
// never need to know about each other; adding one touches only: its own
// file, one #elif branch here, and one -D flag in platformio.ini.

#if defined(INPUT_PS4_DS4_BT)
  #include "PS4_dualshock.h"

// #elif defined(INPUT_ANOTHER_DEVICE)
//   #include "another_input.h"

#elif defined(INPUT_MODULE_NONE)
  #include "nop.h"

#else
  #error "inputs.h: no INPUT_xxx flag defined — set one in platformio.ini (e.g. -D INPUT_PS4_DS4_BT, or -D INPUT_MODULE_NONE for an autonomous machine)."
#endif


// =============================================================================
// INPUT → COMBUS MAPPING STRUCTURES
// =============================================================================

#include <struct/combus_struct.h>  // AnalogComBusID + DigitalComBusID

struct InputAnalogMap {
  AnalogInputDevID devID;
  AnalogComBusID   busChannel;
  bool             isInverted;
};

struct InputDigitalMap {
  DigitalInputDevID devID;
  DigitalComBusID    busChannel;
  bool                isInverted;
};

// EOF inputs.h
