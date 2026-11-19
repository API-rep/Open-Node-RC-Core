/******************************************************************************
 * @file  com_manager.cpp
 * @brief Communication update dispatcher: reception and emission in the main loop.
 *****************************************************************************/

#include "com_manager.h"

#include <core/system/combus/protocol/combus_rx.h>                       // combus_rx_update(), combus_rx_both_or_update()
#include <core/system/combus/protocol/link/combus_link_supervisor.h>     // combus_link_supervisor_update()
#include <core/system/combus/protocol/combus_tx.h>                       // combus_tx_update()



// =============================================================================
// 1. RECEPTION
// =============================================================================

/**
 * @brief Reception pass. Brings the state of the links and of the bus up to date.
 *
 * @details In order:
 *   1. Receiving links: decode the frames, apply them to the bus, merge the both_or
 *      channels (anti ping-pong, both_or.md).
 *   2. Supervisor: couplings, safe values, logs, contributors of the loss.
 */

void com_rx_update(ComBus& bus) {

		// 1. Receiving links
	combus_rx_update();
	combus_rx_both_or_update(bus);

		// 2. Supervisor
	combus_link_supervisor_update(bus);
}



// =============================================================================
// 2. EMISSION
// =============================================================================

	/// Each link keeps its own timer: this call is non-blocking; nothing to do without a sending link.
void com_tx_update(const ComBus& bus) {
	combus_tx_update(&bus);
}

// EOF com_manager.cpp
