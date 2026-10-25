/******************************************************************************
 * @file com_manager.cpp
 * @brief Implementation of the communication update dispatcher.
 *****************************************************************************/

#include "com_manager.h"

#include <core/config/machines/combus_types.h>          // DigitalComBusID::COM_LINK_LOST, FAILSAFE_COM_LINK
#include <core/system/combus/combus_access.h>           // combus_set_digital()
#include <core/system/combus/protocol/combus_rx.h>      // combus_rx_update()
#include <core/system/combus/protocol/combus_links.h>   // combus_links_lost(), combus_links_update()

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
    if (bus.digitalBus[ch].bothOr.enabled) {
      combus_both_or_set_remote(bus, static_cast<DigitalComBusID>(ch), combus_rx_peer_or(ch, CombusRxNoLink));
    }
  }

	// --- Link loss policy (onLoss): log the changes, feed the IDLE and FAILSAFE contributors each cycle ---
  combus_links_update();
#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
  combus_set_digital(bus, DigitalComBusID::COM_LINK_LOST, combus_links_lost(LinkLossAction::IDLE));
#endif
#if defined(HAS_FAILSAFE)
  combus_set_digital(bus, DigitalComBusID::FAILSAFE_COM_LINK, combus_links_lost(LinkLossAction::FAILSAFE));
#endif

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
