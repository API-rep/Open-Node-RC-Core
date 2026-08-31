/*!****************************************************************************
 * @file  inputs_map.h
 * @brief Dumper-truck — Input device to ComBus channel mapping dispatcher.
 *
 * @details This file is the umbrella for input-device-to-ComBus mapping structure
 *   types definition. From -D INPUT_* compile flag, it includes the correct mapping
 *   definition file for the project.
 *
 *   Provided defaults:
 *     - INPUT_PS4_DS4_BT → PS4_dualshock_map.h
 *     - INPUT_NONE        → nop_map.h    (no input device — autonomous mode)
 *
 *   USAGE:
 *     Consumers include this header only — no direct inclusion of
 *     device-specific mapping headers.
 *
 *   A9.9 MIGRATION:
 *     - This dispatcher is now in core/ (was machines/.../volvo_A60H_bruder/inputs_map/).
 *     - The mapping is the default for ANY dumper_truck instance (Cat 770,
 *       Bell B45, Komatsu HD785, ...).
 *     - Per-instance override is left to a future iteration.
 *******************************************************************************///
#pragma once

#include <core/config/inputs/inputs.h>   // pulls InputAnalogMap / InputDigitalMap structs + device vocab


#if defined(INPUT_PS4_DS4_BT)
  #include "PS4_dualshock_map.h"

// #elif defined(INPUT_ANOTHER_DEVICE)
//   #include "another_input_map.h"

#elif defined(INPUT_NONE)
  #include "nop_map.h"

#else
  #error "inputs_map.h: no INPUT_xxx flag defined for dumper_truck. Check platformio.ini."
#endif


// EOF inputs_map.h
