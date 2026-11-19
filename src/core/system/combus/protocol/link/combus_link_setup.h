/******************************************************************************
 * @file  combus_link_setup.h
 * @brief Set-up of the ComBus links from their description: validate, set up the ports, start.
 *
 * @details Turns the ComDevMap table of a board into running links:
 *     1. combus_link_check() validates one ComDevMap;
 *     2. the port is set up and gives its ComPort;
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
 *   Once started, the links are followed by the supervisor (combus_link_supervisor.h),
 *   which also holds the per-link storage (ComLinkRuntime).
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/com_port.h>
#include <core/system/hw/pin_reg.h>
#include <core/system/combus/protocol/link/combus_link_defs.h>
#include <core/system/combus/protocol/link/combus_link_supervisor.h>   // ComLinkRuntime
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>



// =============================================================================
// 1. STORAGE OF THE CALLER
// =============================================================================

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
 *   - a receiving end whose timeout is not longer than the period 1000 / txHz of
 *     the same protocol: the link would drop between two frames; below
 *     kCombusLinkTimeoutWarnPeriods periods, the link is accepted with a warning;
 *   - a line rate above the ceiling of the port;
 *   - a line loaded above kCombusLinkLoadMaxPct (combus_link_load_pct()); above
 *     kCombusLinkLoadWarnPct, the link is accepted with a warning;
 *   - a table on a link between boards, or too large for the RX buffers.
 *
 * @param map  Wiring to validate.
 *
 * @return true if the wiring is acceptable.
 */

bool combus_link_check( const ComDevMap& map );


	/// Load of a line above which a link is accepted with a warning, in % of its rate.
static constexpr uint32_t kCombusLinkLoadWarnPct = 70u;

	/// Load of a line above which a link is refused: the writes would block the loop.
static constexpr uint32_t kCombusLinkLoadMaxPct = 100u;

	/// Timeout of a receiving end, in periods of the frames, below which a link is accepted
	/// with a warning: a single lost frame may already drop it.
static constexpr uint32_t kCombusLinkTimeoutWarnPeriods = 3u;


/**
 * @brief Load of the line of one end, in % of its line rate.
 *
 * @details The full frame of the link at txHz, plus a handshake request and its ACK
 *   per period, 10 bits per byte.  0 for an end that does not transmit, or a port
 *   other than a UART.
 *
 * @param map  Wiring of the link.
 *
 * @return The load, in % (above 100: the line cannot carry it).
 */

uint32_t combus_link_load_pct( const ComDevMap& map );


/**
 * @brief Fill the ComBusLink descriptor of one wired link.
 *
 * @details Runs combus_link_check() first.  The scope gives the layer and the frame
 *   size, or the table of the link; the end gives which of TX and RX are configured.
 *   @p link and @p runtime are only written on success.
 *
 * @param map      Wiring of the link.
 * @param port     Port already set up for `map.portCfg`.
 * @param runtime  Storage of this link.
 * @param link     Descriptor to fill.
 *
 * @return true on success.
 */

bool combus_link_build( const ComDevMap& map,
                        ComPort*         port,
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

// EOF combus_link_setup.h
