/*!****************************************************************************
 * @file  inputs.h
 * @brief Global input module configuration file.
 *
 * @details This file is the single entry point for the input subsystem.
 *   It dispatches to the active input-device module (nop, PS4, RC, etc.)
 *   via INPUT_MODULE and exposes the shared composition structures
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

#ifndef INPUT_MODULE
  #warning "No INPUT_MODULE defined. Falling back to nop input backend."
  #define INPUT_MODULE INPUT_MODULE_NONE
#endif

#if INPUT_MODULE == PS4_DS4_BT
  #include "PS4_dualshock.h"

// #elif INPUT_MODULE == ANOTHER_INPUT_DEVICE
//   #include "another_input.h"

#elif INPUT_MODULE == INPUT_MODULE_NONE
  #include "nop.h"

#else
  #error "Unsupported INPUT_MODULE value. Check platformio.ini or INPUT_MODULE definition."
#endif


// =============================================================================
// INPUT → COMBUS MAPPING STRUCTURES
// =============================================================================

#include <struct/combus_struct.h>  //AnalogComBusID + DigitalComBusID

/** 
 * @brief Analog input device → ComBus channel mapping entry.
 *
 * @details Domain-specific composition structure — combines the input device
 *   vocabulary (AnalogInputDevID, dispatched above by INPUT_MODULE) with the
 *   ComBus channel vocabulary (AnalogComBusID, struct/combus_struct.h).
 *   Only meaningful within the input domain — not exposed in include/.
 *
 *   Field order matters: consumers may use positional initialisation
 *   (see PS4_dualshock_map.cpp).
 */
struct InputAnalogMap {
    AnalogInputDevID devID;     ///< Input device channel (e.g. LY_STICK)
    AnalogComBusID   busChannel; ///< Target ComBus channel (e.g. DRIVE_SPEED_BUS)
    bool             isInverted; ///< Signal inversion flag
};

/**
 * @brief Digital input device → ComBus channel mapping entry.
 *
 * @details See InputAnalogMap for rationale.
 */
struct InputDigitalMap {
    DigitalInputDevID devID;     ///< Input device channel (e.g. CROSS_BTN)
    DigitalComBusID   busChannel; ///< Target ComBus channel (e.g. HORN)
    bool              isInverted; ///< Signal inversion flag
};


// EOF inputs.h
