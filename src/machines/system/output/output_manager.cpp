/******************************************************************************
 * @file output_manager.cpp
 * @brief Implementation of the output update dispatcher.
 *****************************************************************************/

#include "output_manager.h"

#ifdef SOUND_OUTPUT_UART
  #include <core/system/com/protocols/combus_tx.h>
#endif


// =============================================================================
// 1. OUTPUT DISPATCH
// =============================================================================

/**
 * @brief Dispatch all active output module updates.
 */
void output_update(const ComBus& bus, bool failsafeActive) {

	// --- Sound node UART TX (50 Hz timer-gated, non-blocking) ---
#ifdef SOUND_OUTPUT_UART
  combus_tx_update(&bus, failsafeActive);
#endif

}

// EOF output_manager.cpp
