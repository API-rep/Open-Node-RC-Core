/*!****************************************************************************
 * @file  com.h
 * @brief Communication transport config — module selector.
 *
 * @details Includes the sub-config for each active communication module based on
 *   build flags. Follows the same pattern as inputs/inputs.h and vbat/config.h.
 *   Communication modules are optional — no compile error if no flag is set.
 *   Channel constants and static_asserts live in each sub-file.
 *******************************************************************************///
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>
// NOTE: board constants (SerialExt, TxdExtPin, UartMaxBaud …) are available because
//       com/com.h is always included via output_init.h → init.h,
//       by which point the board header is already in scope.


// =============================================================================
// 1. OUTPUT MODULE SELECTION
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  #include "combus_uart.h"
#endif

// EOF com.h
