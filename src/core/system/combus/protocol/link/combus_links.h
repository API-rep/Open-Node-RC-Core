/******************************************************************************
 * @file  combus_links.h
 * @brief ComBus links from their description: validate, open the ports, start.
 *
 * @details Turns the ComDevMap table of a board into running links:
 *     1. combus_link_check() validates one ComDevMap;
 *     2. the port is opened and gives a NodeCom;
 *     3. combus_link_build() fills the ComBusLink descriptor of that link;
 *     4. the TX and RX pools are registered and each link is started.
 *
 *   combus_link_check() and combus_link_build() touch no hardware: they can be tested
 *   on the target without any wiring.  All storage is static and owned by the caller:
 * @code
 *   static CombusTxState  txStates[1];
 *   static CombusRxState  rxStates[1];
 *   static ComLinkRuntime runtime[1];
 *   static ComBusLink     links[1];
 *   const ComLinksStorage storage = { txStates, rxStates, runtime, links };
 *
 *   const ComDevMap maps[] = { { &kComDevExtBi, &comPorts[0], LinkEnd::TX_RX } };
 *   combus_links_init(maps, 1, storage, &pinReg, &bus);
 * @endcode
 *
 *   Once started, the links are watched by combus_link_watch.h.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/hw/pin_reg.h>
#include <core/config/machines/combus_types.h>   // AnalogComBusID, DigitalComBusID
#include "combus_system_ids.h"                   // size of the SYSTEM view
#include <core/system/combus/protocol/link/combus_link_defs.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>
#include <core/system/combus/protocol/coupling/combus_pairing.h>    // ComLinkPairing



// =============================================================================
// 1. PER-LINK STORAGE
// =============================================================================

/**
 * @brief What the watch of a link needs.
 */

struct ComLinkWatch {
	bool     receives       = false;   ///< this link has an RX side
	uint32_t linkTimeoutMs  = 0u;      ///< silence after which a receiving link is lost (ms)
	uint32_t startupGraceMs = 0u;      ///< link between boards: wait for a first frame before it counts as lost (ms)
	uint32_t watchStartMs   = 0u;      ///< start of the grace: set when the watch starts, and at the hitch
	bool     reportedLost   = false;   ///< last loss state logged
	bool     wasAlive       = false;   ///< alive at the last update, to catch the loss
};


/**
 * @brief Coupling of a link ON_COUPLING.
 */

struct ComLinkCoupling {
	const CouplingDev* dev     = nullptr;               ///< link ON_COUPLING; nullptr = always active
	ComLinkRole        role    = ComLinkRole::SOCKET;   ///< side of this node, from its coupler (ComPeerLink::role)
	ComLinkPairing     pairing = {};                    ///< pairing of the two ends
	bool               coupled = false;                 ///< paired (PairingState::COUPLED) at the last update
};


/**
 * @brief Per-link storage that the protocol layers keep pointing to.
 *
 * @details Holds the handshake context shared by TX and RX of the link, the RX decode
 *   buffers, and what the watch needs.  The buffers are sized to the SYSTEM next set
 *   (BASE stable channels, candidates, SYSTEM channels), the largest set that travels:
 *   they fit any link, BASE being a prefix of it.  BOARD channels never travel and get
 *   no room.  The limit accepted on a link is the set of that link (its rxCfg), not
 *   the buffer size.
 *
 *   The name, the scope and the table of the link are read from its ComDev, never
 *   copied: the description stays const, in Flash.
 */

struct ComLinkRuntime {
		// The link
	const ComDev* dev = nullptr;   ///< description of the link; nullptr = never built
	NodeCom*      com = nullptr;   ///< transport: the pairing writes its coupling frames to it

		// State: contract, watch, coupling
	CombusHandshakeContext handshake = {};
	ComLinkWatch           watch     = {};
	ComLinkCoupling        coupling  = {};

		// RX decode buffers, sized to the SYSTEM next set
	uint16_t analogRx[static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT)]   = {};
	bool     digitalRx[static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT)] = {};
};


/**
 * @brief The arrays owned by the caller, used by combus_links_init().
 *
 * @details All four must have the same length, at least the number of links, and
 *   static storage duration.
 */

struct ComLinksStorage {
	CombusTxState*  tx;         ///< TX state pool, registered with combus_tx_register_pool()
	CombusRxState*  rx;         ///< RX state pool, registered with combus_rx_register_pool()
	ComLinkRuntime* runtime;    ///< handshake context and RX buffers, one per link
	ComBusLink*     links;      ///< link descriptors, one per link
};



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Channel set carried by a link of the given scope.
 *
 * @param scope  Scope of the link.
 *
 * @return ChanLayer::BASE for INTER_NODE, ChanLayer::SYSTEM for INTRA_NODE.
 */

constexpr ChanLayer linkScopeLayer( LinkScope scope )
{
	return (scope == LinkScope::INTER_NODE) ? ChanLayer::BASE : ChanLayer::SYSTEM;
}


/**
 * @brief Validate one ComDevMap, before any port is opened.
 *
 * @details Refused, and logged:
 *   - a null device, port or protocol;
 *   - a link ON_COUPLING without its CouplingDev or its table;
 *   - a BIDIRECTIONAL link not wired TX_RX, or a UNIDIRECTIONAL one wired TX_RX;
 *   - a transmitting end with a rate of 0, or above 1000 Hz (period of 0 ms);
 *   - a receiving end without a link timeout;
 *   - a line rate above the ceiling of the port;
 *   - a table on a link between boards, or too large for the RX buffers.
 *
 * @param map  Wiring to validate.
 *
 * @return true if the wiring is acceptable.
 */

bool combus_link_check( const ComDevMap& map );


/**
 * @brief Fill the ComBusLink descriptor of one wired link.
 *
 * @details Runs combus_link_check() first.  The scope gives the layer and the frame
 *   size, or the table of the link; the end gives which of TX and RX are configured.
 *   @p link and @p runtime are only written on success.
 *
 * @param map      Wiring of the link.
 * @param com      Transport already opened for `map.port`.
 * @param runtime  Storage of this link.
 * @param link     Descriptor to fill.
 *
 * @return true on success.
 */

bool combus_link_build( const ComDevMap& map,
                        NodeCom*         com,
                        ComLinkRuntime&  runtime,
                        ComBusLink&      link );


/**
 * @brief Bring up every link of a ComDevMap table.
 *
 * @details For each map: validate, open the port, build the link.  Then the pools are
 *   registered, the links started, and the links put under watch.  A map that fails is
 *   logged and left inactive; the others still start.
 *
 * @param maps     Table of wirings.
 * @param count    Number of entries in @p maps.
 * @param storage  Arrays owned by the caller, at least @p count long.
 * @param pinReg   Optional pin registry, used to claim the pins of the ports.
 * @param target   ComBus that receives the frames accepted on the receiving links.
 *                 nullptr: the frames are decoded but not applied.
 *
 * @return true if every link was brought up.
 */

bool combus_links_init( const ComDevMap*       maps,
                        uint8_t                count,
                        const ComLinksStorage& storage,
                        PinReg*                pinReg = nullptr,
                        ComBus*                target = nullptr );


/**
 * @brief Start every link of an array, in order.
 *
 * @details The TX and RX pools must already be registered.
 *
 * @param links  Array of descriptors, owned by the caller.
 * @param count  Number of entries in @p links.
 */

void combus_links_start_all( ComBusLink* links, uint8_t count );


/**
 * @brief Start one link over its open transport: its contract, its TX side, its RX side.
 *
 * @details Does nothing if @p link or its transport is null, or without a handshake
 *   context (logged).  A link without txCfg nor rxCfg stays inactive.  A contract always
 *   starts UNKNOWN.
 *
 * @warning A layer other than BASE or SYSTEM is a configuration error: the system halts.
 *
 * @param linkIdx  Slot of this link in the TX and RX pools.
 * @param link     Filled descriptor.  Its RX buffers and context must outlive the program.
 */

void combus_link_start( uint8_t linkIdx, ComBusLink* link );

// EOF combus_links.h
