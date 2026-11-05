/******************************************************************************
 * @file  combus_link_watch.h
 * @brief Watch of the running links: their state, their loss, and their safe values.
 *
 * @details Each cycle, combus_links_update() follows the coupling of each link, catches
 *   the loss of a receiving link and logs its changes.  The dashboards read copies,
 *   never the internal structures.  The coupling itself: coupling/combus_coupling_watch.h.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/link/combus_links.h>



// =============================================================================
// 1. STATE OF A LINK
// =============================================================================

/**
 * @brief State of a receiving link.
 */

enum class ComLinkState : uint8_t {
	NEVER_SEEN,     ///< no frame since the link was put under watch, or the link does not receive
	ALIVE,          ///< last frame younger than linkTimeoutMs
	LOST            ///< seen, then silent for longer than linkTimeoutMs
};


/**
 * @brief Read-only snapshot of a watched link, for the dashboards.
 *
 * @details A copy taken at the call.  `peer` and `coupling` point to the generated or
 *   configured descriptors, which are const.
 */

struct ComLinkView {
	const char*        name           = nullptr;
	LinkScope          scope          = LinkScope::INTRA_NODE;
	bool               receives       = false;   ///< the link has an RX side
	uint32_t           linkTimeoutMs  = 0u;
	uint32_t           startupGraceMs = 0u;
	ComLinkState       state          = ComLinkState::NEVER_SEEN;
	bool               contractStable = false;   ///< handshake contract STABLE
	bool               nextMode       = false;   ///< STABLE in next mode (candidates)
	uint32_t           rxAgeMs        = 0u;      ///< ms since the last frame accepted (meaningful once seen)
	const ComPeerLink* peer           = nullptr; ///< link through a coupler: coupler, side, MD5, tables
	const CouplingDev* coupling       = nullptr; ///< coupled link (ON_COUPLING), nullptr otherwise
	ComLinkPairing     pairing        = {};      ///< copy of its pairing state (coupled link)
};



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Put the links of a runtime array under watch.
 *
 * @details Called by combus_links_init(); exposed so that a test can watch links it
 *   built itself.  The startup grace of each link starts here, and a coupled link starts
 *   off until it is paired.
 *
 * @param runtime  Per-link storage, one entry per link, in link order.  Static storage.
 * @param count    Number of entries in @p runtime.
 */

void combus_links_watch( ComLinkRuntime* runtime, uint8_t count );


/**
 * @brief Follow the links, once per cycle.
 *
 * @details Called by com_rx_update().  For each link:
 *   - a coupled link has its pairing stepped and its contributors written;
 *   - a link with a table that stops being alive sends the positions it receives back
 *     to their safe value (CbusNeutral, false).  The positions it sends do not move, a
 *     both_or is never written by a frame, and a link without a table is unchanged;
 *   - the loss and the recovery are logged once per change.  Lost here means lost, or
 *     never seen once startupGraceMs has elapsed, whatever the scope.
 *
 * @param bus  ComBus the received positions and the contributors are written to.
 */

void combus_links_update( ComBus& bus );


/**
 * @brief State of one watched link.
 *
 * @param linkIdx  Index of the link, in the order of the runtime array.
 *
 * @return NEVER_SEEN, ALIVE or LOST; NEVER_SEEN for a link that does not receive, or an
 *   index out of range.
 */

ComLinkState combus_link_state( uint8_t linkIdx );


/**
 * @brief True if a receiving link of the given scope counts as lost.
 *
 * @details The failsafe chain that reads the contributor decides the severity:
 *   - INTRA_NODE (BOARD_LINK_FAIL): the link is lost, or never seen once startupGraceMs
 *     has elapsed, since the boards start together;
 *   - INTER_NODE (NODE_LINK_FAIL): the link is not alive, never seen included, with no
 *     grace: the other node may not be there.
 *
 *   A coupled link never counts here: its loss goes to the contributor of its
 *   CouplingDev.  A link that only transmits never counts: it cannot know its peer.
 *
 * @param scope  Scope of the links to look at.
 *
 * @return true if at least one receiving link of this scope counts as lost.
 */

bool combus_links_lost( LinkScope scope );


/**
 * @brief Send the positions received on a link back to their safe value.
 *
 * @details CbusNeutral for an analog channel, false for a digital one.  A both_or
 *   position is left out: a frame never writes it, and the merge ignores a link that is
 *   not alive.
 *
 * @param peer  Table of the link.
 * @param bus   ComBus the positions are written to.
 */

void combus_link_take_back_safe_values( const ComPeerLink& peer, ComBus& bus );


/**
 * @brief Pairing state of a coupled link.
 *
 * @param linkIdx  Index of the link.
 *
 * @return The state; DETACHED for a link that is not coupled, or an index out of range.
 */

PairingState combus_links_pairing_state( uint8_t linkIdx );



// =============================================================================
// 3. SNAPSHOTS FOR THE DASHBOARDS
// =============================================================================

/**
 * @brief Number of links under watch.
 *
 * @return The number of links; 0 if there is no watch.
 */

uint8_t combus_links_count( void );


/**
 * @brief Snapshot of a link under watch.
 *
 * @param linkIdx  Index of the link.
 * @param out      Copy of its state.
 *
 * @return false if @p linkIdx is out of range.
 */

bool combus_links_view( uint8_t linkIdx, ComLinkView& out );

// EOF combus_link_watch.h
