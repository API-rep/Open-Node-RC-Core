/******************************************************************************
 * @file com_manager.cpp
 * @brief Implementation of the communication update dispatcher.
 *****************************************************************************/

#include "com_manager.h"

#include <core/config/machines/combus_types.h>          // DigitalComBusID::UART_LINK_LOST
#include <core/system/combus/combus_access.h>           // combus_set_digital()
#include <core/system/combus/protocol/combus_rx.h>      // combus_rx_update()
#include <core/system/combus/protocol/combus_links.h>   // combus_links_rx_lost()

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  #include <core/system/combus/protocol/combus_tx.h>
#endif


// =============================================================================
// 1. RECEPTION
// =============================================================================

void com_rx_update(ComBus& bus) {

	// --- ComBus receiving links: decode and apply the frames ---
  combus_rx_update(&bus);

	// --- both_or channels: value = this board's value OR the live links (anti ping-pong, both_or.md) ---
  for (uint8_t ch = 0u; ch < static_cast<uint8_t>(DigitalComBusID::CH_COUNT); ++ch) {
    if (bus.digitalBus[ch].bothOr) {
      combus_both_or_set_remote(bus, static_cast<DigitalComBusID>(ch), combus_rx_peer_or(ch, CombusRxNoLink));
    }
  }

	// --- Link health: the proof of life of the REMOTE_LINK_LOST chain, re-armed each cycle ---
  combus_set_digital(bus, DigitalComBusID::UART_LINK_LOST, combus_links_rx_lost());

}


// =============================================================================
// 2. EMISSION
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
