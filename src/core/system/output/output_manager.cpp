/******************************************************************************
 * @file output_manager.cpp
 * @brief Implementation of the output update dispatcher.
 *****************************************************************************/

#include "output_manager.h"

#include <core/system/com/com_manager.h>   // com_update() — communication modules


// =============================================================================
// 1. OUTPUT DISPATCH
// =============================================================================

/**
 * @brief Dispatch all active output module updates.
 */
void output_update(const ComBus& bus) {

	// --- Communication modules (ComBus TX, ...) ---
  com_update(bus);

}

// EOF output_manager.cpp
