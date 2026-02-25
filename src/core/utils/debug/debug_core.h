/*****************************************************************************
 * @file debug_core.h
 * @brief Core debug theme switch for all sub-projects
 *
 * @details This file provides a minimal and evolutive debug architecture:
 * - common theme switches live in core
 * - specific debug code remains in its module (machine, remote, etc.)
 * - activation is done only through DEBUG_THEME_* flags
 *****************************************************************************/
#pragma once

// =============================================================================
// 1. GLOBAL THEME SWITCHES
// =============================================================================

// Expand one single flag into all available debug themes.
// Useful for bring-up sessions where full verbose traces are required.

#if defined(DEBUG_THEME_ALL)
  #define DEBUG_THEME_INPUT
  #define DEBUG_THEME_HW
  #define DEBUG_THEME_SYSTEM
  #define DEBUG_THEME_COMBUS
#endif

// =============================================================================
// 2. THEME STATE FLAGS
// =============================================================================

// Convert preprocessor theme definitions to numeric constants.
// These constants are easier to reuse in #if expressions.

#if defined(DEBUG_THEME_INPUT)
  #define DEBUG_THEME_INPUT_ENABLED 1
#else
  #define DEBUG_THEME_INPUT_ENABLED 0
#endif

#if defined(DEBUG_THEME_HW)
  #define DEBUG_THEME_HW_ENABLED 1
#else
  #define DEBUG_THEME_HW_ENABLED 0
#endif

#if defined(DEBUG_THEME_SYSTEM)
  #define DEBUG_THEME_SYSTEM_ENABLED 1
#else
  #define DEBUG_THEME_SYSTEM_ENABLED 0
#endif

#if defined(DEBUG_THEME_COMBUS)
  #define DEBUG_THEME_COMBUS_ENABLED 1
#else
  #define DEBUG_THEME_COMBUS_ENABLED 0
#endif

// =============================================================================
// 3. THEME UTILITY WRAPPERS
// =============================================================================

// Execute code only when the corresponding theme is enabled.
// These wrappers keep call sites concise and avoid repeated #if/#endif blocks.

#if DEBUG_THEME_INPUT_ENABLED
  #define DBG_IF_INPUT(code) do { code; } while(0)
#else
  #define DBG_IF_INPUT(code) do { } while(0)
#endif

#if DEBUG_THEME_HW_ENABLED
  #define DBG_IF_HW(code) do { code; } while(0)
#else
  #define DBG_IF_HW(code) do { } while(0)
#endif

#if DEBUG_THEME_SYSTEM_ENABLED
  #define DBG_IF_SYSTEM(code) do { code; } while(0)
#else
  #define DBG_IF_SYSTEM(code) do { } while(0)
#endif

#if DEBUG_THEME_COMBUS_ENABLED
  #define DBG_IF_COMBUS(code) do { code; } while(0)
#else
  #define DBG_IF_COMBUS(code) do { } while(0)
#endif

// EOF debug_core.h
