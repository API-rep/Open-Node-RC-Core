/******************************************************************************
 * @file  remote_node_source.cpp
 * @brief The remote of the project as a command source: its link, read by a module.
 *****************************************************************************/

#if defined(HAS_REMOTE_NODE)

#include "remote_node_source.h"

#include <core/system/combus/protocol/combus_rx.h>                       // combus_rx_snapshot(), combus_rx_apply_snapshot()
#include <core/system/combus/protocol/link/combus_link_supervisor.h>     // combus_link_state(), combus_link_take_back_safe_values()



// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Alive pass. The state of the link; the safe values at its loss.
 *
 * @details In order:
 *   1. Alive: the link is alive and its snapshot valid (a contract dropped invalidates it).
 *   2. The loss, once: the positions received back to their safe value, with the
 *      identity of the board.  The base sends the inputs back too, every cycle while no
 *      source drives; the relayed commands have no other writer.
 */

bool remote_node_poll( ComBus& bus, void* ctx )
{
	RemoteNodeSource& src = *static_cast<RemoteNodeSource*>(ctx);

		// 1. Alive
	const bool alive = (combus_link_state(src.linkIdx) == ComLinkState::ALIVE)
	                && (combus_rx_snapshot(src.linkIdx) != nullptr);

		// 2. The loss
	if (src.wasAlive && !alive && src.peer) { combus_link_take_back_safe_values(*src.peer, bus, ChanLayer::BOARD); }

	src.wasAlive = alive;

	return alive;
}


	/// The table of the link decides what is written: the positions this node receives.
void remote_node_apply( ComBus& bus, void* ctx )
{
	const RemoteNodeSource& src = *static_cast<RemoteNodeSource*>(ctx);

	combus_rx_apply_snapshot(src.linkIdx, bus, ChanLayer::BOARD);
}

#endif  // HAS_REMOTE_NODE

// EOF remote_node_source.cpp
