/******************************************************************************
 * @file output_init.cpp
 * @brief Implementation of output peripherals initialization.
 *****************************************************************************/

#include "output_init.h"
#include "../com/com_init.h"
#include <machines/config/config.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. OUTPUT INITIALIZATION
// =============================================================================

/**
 * @brief Initialise output transport layer.
 *
 * @details Each output module is conditionally compiled by build flags.
 * Adding a new output peripheral means adding its init call here, guarded by its
 * own build flag. Multiple non conflictual modules can be active at the same time
 * (ex: UART + wireless outputs).
 * Pin assignments come from the active environment configuration (ex: boards/*.h).
 */

void output_init() {
  sys_log_info("[OUTPUT] output_init ...\n");

  com_init();    // no-op if no transport flag is defined

  sys_log_info("[OUTPUT] output_init done\n\n");
}

// EOF output_init.cpp
