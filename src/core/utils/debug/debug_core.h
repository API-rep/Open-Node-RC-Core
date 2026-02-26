/*****************************************************************************
 * @file debug_core.h
 * @brief Core debug theme switch for all sub-projects
 *
 * @details This file provides a minimal and evolutive debug architecture:
 * - common theme switches live in core
 * - specific debug code remains in its module (machine, remote, etc.)
 * - activation is done only through DEBUG_* flags
 *****************************************************************************/
#pragma once

#include <stdint.h>

// =============================================================================
// 1. GLOBAL THEME SWITCHES
// =============================================================================

// Expand one single flag into all available debug themes.
// Useful for bring-up sessions where full verbose traces are required.

#if defined(DEBUG_ALL)
  #define DEBUG_INPUT
  #define DEBUG_HW
  #define DEBUG_SYSTEM
  #define DEBUG_COMBUS
#endif

// =============================================================================
// 2. THEME STATE FLAGS
// =============================================================================

// Convert preprocessor theme definitions to numeric constants.
// These constants are easier to reuse in #if expressions.

#if defined(DEBUG_INPUT)
  #define DEBUG_INPUT_ENABLED 1
#else
  #define DEBUG_INPUT_ENABLED 0
#endif

#if defined(DEBUG_HW)
  #define DEBUG_HW_ENABLED 1
#else
  #define DEBUG_HW_ENABLED 0
#endif

#if defined(DEBUG_SYSTEM)
  #define DEBUG_SYSTEM_ENABLED 1
#else
  #define DEBUG_SYSTEM_ENABLED 0
#endif

#if defined(DEBUG_COMBUS)
  #define DEBUG_COMBUS_ENABLED 1
#else
  #define DEBUG_COMBUS_ENABLED 0
#endif

#if (DEBUG_INPUT_ENABLED || DEBUG_HW_ENABLED || DEBUG_SYSTEM_ENABLED || DEBUG_COMBUS_ENABLED)
  #define DEBUG_ANY_ENABLED 1
#else
  #define DEBUG_ANY_ENABLED 0
#endif

// =============================================================================
// 3. CORE DEBUG BOOTSTRAP API
// =============================================================================

/**
 * @brief Shared baudrate used by debug serial monitor
 *
 * @details
 * - this value is expected to be injected from platformio.ini using
 *   -D DEBUG_MONITOR_BAUD=${env.monitor_speed}
 * - fallback keeps compatibility if the build flag is missing
 */

#ifndef DEBUG_MONITOR_BAUD
  #define DEBUG_MONITOR_BAUD 115200
#endif

/**
 * @brief Initialize debug serial monitor once when debug is enabled
 *
 * @details
 * This routine centralizes serial monitor bootstrap for debug outputs.
 * It is safe to call multiple times: initialization occurs only once.
 *
 * Initialization policy:
 * - if no debug theme is enabled, this function compiles to a no-op
 * - if serial monitor is already initialized, it does nothing
 * - otherwise it initializes Serial with DEBUG_MONITOR_BAUD
 */

void debugCoreInit();

// =============================================================================
// 4. THEME UTILITY WRAPPERS
// =============================================================================

// Execute code only when the corresponding theme is enabled.
// These wrappers keep call sites concise and avoid repeated #if/#endif blocks.

#if DEBUG_INPUT_ENABLED
  #define DBG_IF_INPUT(code) do { code; } while(0)
#else
  #define DBG_IF_INPUT(code) do { } while(0)
#endif

#if DEBUG_HW_ENABLED
  #define DBG_IF_HW(code) do { code; } while(0)
#else
  #define DBG_IF_HW(code) do { } while(0)
#endif

#if DEBUG_SYSTEM_ENABLED
  #define DBG_IF_SYSTEM(code) do { code; } while(0)
#else
  #define DBG_IF_SYSTEM(code) do { } while(0)
#endif

#if DEBUG_COMBUS_ENABLED
  #define DBG_IF_COMBUS(code) do { code; } while(0)
#else
  #define DBG_IF_COMBUS(code) do { } while(0)
#endif

// EOF debug_core.h
