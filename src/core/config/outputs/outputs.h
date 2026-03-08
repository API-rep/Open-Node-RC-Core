/*!****************************************************************************
 * @file  outputs.h
 * @brief Output transport config — module selector.
 *
 * @details Includes the sub-config for each active output module based on
 *   build flags. Follows the same pattern as inputs/inputs.h and vbat/config.h.
 *   Output modules are optional — no compile error if no flag is set.
 *   Channel constants and static_asserts live in each sub-file.
 *******************************************************************************///
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>
// NOTE: board constants (SoundUartBaud, Txd1Pin …) are available because
//       outputs/outputs.h is always included via output_init.h → init.h,
//       by which point the board header is already in scope.


// =============================================================================
// 1. OUTPUT MODULE SELECTION
// =============================================================================

#ifdef SOUND_OUTPUT_UART
  #include "sound_uart.h"
#endif

// EOF outputs.h
