/******************************************************************************
 * @file  com_manager.cpp
 * @brief Communication update dispatcher: reception and emission in the main loop.
 *****************************************************************************/

#include "com_manager.h"

#include <core/config/machines/combus_types.h>                      // DigitalComBusID::NODE_LINK_FAIL, BOARD_LINK_FAIL
#include <core/system/combus/combus_access.h>                       // combus_set_digital()
#include <core/system/combus/protocol/combus_rx.h>                  // combus_rx_update()
#include <core/system/combus/protocol/link/combus_link_watch.h>     // combus_links_lost(), combus_links_update()

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
	#include <core/system/combus/protocol/combus_tx.h>
#endif



// =============================================================================
// 1. RECEPTION
// =============================================================================

/**
 * @brief Reception pass. Brings the state of the links and of the bus up to date.
 *
 * @details In order:
 *   1. Receiving links: decode the frames, and apply them to the bus.
 *   2. both_or channels: this board's value OR the live links (anti ping-pong, both_or.md).
 *   3. Watch of the links: couplings, safe values, logs.
 *   4. Contributors of the loss, by scope, written on every cycle.
 */

void com_rx_update(ComBus& bus) {

		// 1. Receiving links
	combus_rx_update();

		// 2. both_or channels
	for (uint8_t ch = 0u; ch < static_cast<uint8_t>(DigitalComBusID::CH_COUNT); ++ch) {
		if (bus.digitalBus[ch].bothOr.enabled) {
			combus_both_or_set_remote(bus, static_cast<DigitalComBusID>(ch), combus_rx_peer_or(ch, CombusRxNoLink));
		}
	}

		// 3. Watch of the links
	combus_links_update(bus);

		// 4. Contributors of the loss
#if defined(HAS_FAILSAFE) && defined(HAS_NODE_LINK_FAILSAFE)
	combus_set_digital(bus, DigitalComBusID::NODE_LINK_FAIL, combus_links_lost(LinkScope::INTER_NODE));
#endif
#if defined(HAS_FAILSAFE) && defined(HAS_BOARD_LINK_FAILSAFE)
	combus_set_digital(bus, DigitalComBusID::BOARD_LINK_FAIL, combus_links_lost(LinkScope::INTRA_NODE));
#endif

}



// =============================================================================
// 2. EMISSION
// =============================================================================

	/// Each link keeps its own timer: this call is non-blocking.
void com_update(const ComBus& bus) {

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
	combus_tx_update(&bus);
#endif

}

// EOF com_manager.cpp
