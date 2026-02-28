/******************************************************************************
 * @file input_init.cpp
 * @brief Implementation of input system initialization
 *****************************************************************************/

#include "input_init.h"
#include <core/utils/debug/debug.h>
#include <core/utils/input/input_manager.h>


// =============================================================================
// 1. INPUT INITIALIZATION
// =============================================================================

/**
 * @brief Input system init — input_setup() with init-sequence log output.
 */
void input_init() {
  sys_log_info("[INPUT] Input init...\n");
  input_setup();
  sys_log_info("[INPUT] Input init complete\n");
}

// EOF input_init.cpp
