/******************************************************************************
 * @file com_manager.cpp
 * @brief Implementation of the communication update dispatcher.
 *****************************************************************************/

#include "com_manager.h"

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  #include <core/system/combus/protocol/combus_tx.h>
#endif


// =============================================================================
// 1. COMMUNICATION DISPATCH
// =============================================================================

/**
 * @brief Dispatch all active communication module updates.
 */

void com_update(const ComBus& bus) {

	// --- ComBus UART TX (50 Hz timer-gated, non-blocking) ---
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  combus_tx_update(&bus);
#endif

}

// EOF com_manager.cpp
