/*!****************************************************************************
 * @file    mainboard.h
 * @brief   Volvo A60H Bruder — machine-env umbrella (dispatches on BOARD).
 *
 * @details Includes the vehicle-level shared constants from the Level-0 header
 *   then selects the board-specific environment configuration based on the
 *   -D BOARD build flag.
 *
 *   Architecture:
 *   @code
 *   machines.h
 *     └── volvo_A60H_bruder/mainboard/mainboard.h   ← this file
 *           ├── ../volvo_A60H_bruder.h               (Level 0 — shared identity)
 *           └── ESP32_8M_6S/envCfg.h                 (board-specific config)
 *   @endcode
 *
 *   To add a new board: add an `#elif BOARD == <YOUR_BOARD>` branch and create
 *   the matching board folder under `mainboard/`.
 *******************************************************************************
 */
#pragma once

#include "../volvo_A60H_bruder.h"   // kVehicleName, kVehicleCombusLayout

// =============================================================================
// Board selection
// =============================================================================

#ifndef BOARD
  #define BOARD  ESP32_8M_6S  ///< Default board if not set by build flags.
#endif

#if BOARD == ESP32_8M_6S
  #include "ESP32_8M_6S/envCfg.h"
#else
  #error "Unknown BOARD for volvo_A60H_bruder mainboard. Check -D BOARD in platformio.ini."
#endif

// EOF mainboard.h
