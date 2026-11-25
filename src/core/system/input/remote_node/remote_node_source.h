/******************************************************************************
 * @file  remote_node_source.h
 * @brief The remote of the project as a command source: its link, read by a module.
 *
 * @details The link of the machine with its remote is a command source (ComDev::source):
 *   it keeps all it has (handshake, table, MD5, age, loss), but writes no bus.  Its
 *   frames stay in the snapshot, which this module reads:
 *   - alive: the link is alive, with a valid snapshot;
 *   - mapper: the table the builder generates for the link (kRemoteLink), applied to
 *     the snapshot with the identity of the board (combus_rx_apply_snapshot()): the
 *     inputs of the type, and the commands a coupler relays;
 *   - at the loss of the link, the module sends every position it receives back to its
 *     safe value, the relayed commands included: the base takes back the inputs only.
 *   Plugged into the base (remote_source.h) through remote_node_poll() and
 *   remote_node_apply(), with a RemoteNodeSource as context.
 *   Design: doc/WIP - Local remote.md §2, §4.
 *****************************************************************************/
#pragma once

#if defined(HAS_REMOTE_NODE)

#if !defined(HAS_REMOTE)
#error "HAS_REMOTE_NODE needs HAS_REMOTE: a command source writes the inputs of the board that acquires the commands"
#endif

#include <stdint.h>
#include <stdbool.h>

#include <core/config/machines/combus_types.h>                        // ComBus
#include <core/system/combus/protocol/link/combus_peer_link_defs.h>   // ComPeerLink



// =============================================================================
// 1. THE MODULE
// =============================================================================

/**
 * @brief The remote module: which link, its table, and what it saw last.
 */

struct RemoteNodeSource {
	uint8_t            linkIdx;            ///< slot of the link in the pool (order of the board table)
	const ComPeerLink* peer;               ///< table of the link (ComDev::peer)
	bool               wasAlive = false;   ///< alive at the last poll, to catch the loss
};



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief RemoteSourceDev::poll of the remote. Alive while the link is.
 *
 * @details Alive: the link is ALIVE (combus_link_state()) and its snapshot valid.  On
 *   the loss (alive, then not), every position received goes back to its safe value.
 *
 * @param bus  ComBus the safe values are written to, at the loss.
 * @param ctx  The RemoteNodeSource.
 *
 * @return true if the remote is alive.
 */

bool remote_node_poll( ComBus& bus, void* ctx );


/**
 * @brief RemoteSourceDev::apply of the remote. The snapshot to the bus, through the table.
 *
 * @param bus  ComBus the positions received are written to.
 * @param ctx  The RemoteNodeSource.
 */

void remote_node_apply( ComBus& bus, void* ctx );

#endif  // HAS_REMOTE_NODE

// EOF remote_node_source.h
