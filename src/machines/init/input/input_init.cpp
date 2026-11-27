/******************************************************************************
 * @file input_init.cpp
 * @brief Implementation of input system initialization
 *****************************************************************************/

#include "input_init.h"
#include <core/system/debug/logging/debug.h>
#include <machines/system/input/input_sources.h>   // input_sources_begin()


// =============================================================================
// 1. INPUT INITIALIZATION
// =============================================================================

/**
 * @brief Input system init — the command source modules, with init-sequence log output.
 */
void input_init() {
  sys_log_info("[INPUT] Input init...\n");
  input_sources_begin();
  sys_log_info("[INPUT] Input init complete\n\n");
}

// EOF input_init.cpp
