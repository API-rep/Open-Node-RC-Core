/******************************************************************************
 * @file combus_links.h
 * @brief ComBus links from their description — open the ports, build the links.
 *
 * @details Turns a board's ComDevMap table (a ComDev wired on a ComPort, see
 *   combus_protocol.h) into running ComBus links:
 *     1. combus_link_check()  validates one ComDevMap;
 *     2. the port is opened (UART today) and gives a NodeCom*;
 *     3. combus_link_build()  fills the ComBusLink descriptor of that link;
 *     4. the TX and RX pools are registered and combus_protocol_init_all()
 *        brings every link up.
 *
 *   combus_link_check() and combus_link_build() touch no hardware: they can
 *   be tested on the target without any wiring.
 *
 *   All storage is caller-owned and static (nothing is allocated here):
 * @code
 *   static CombusTxState  txStates[1];
 *   static CombusRxState  rxStates[1];
 *   static ComLinkRuntime runtime[1];
 *   static ComBusLink     links[1];
 *   const ComLinksStorage storage = { txStates, rxStates, runtime, links };
 *
 *   const ComDevMap maps[] = { { &kComDevExtBi, &comPorts[0], LinkEnd::TX_RX } };
 *   combus_links_init(maps, 1, storage, &pinReg);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/hw/pin_reg.h>
#include <core/config/machines/combus_types.h>   // AnalogComBusID / DigitalComBusID (FULL view sizes)
#include <core/system/combus/protocol/combus_protocol.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>


// =============================================================================
// 1. PER-LINK STORAGE
// =============================================================================

/**
 * @brief Per-link storage that the protocol layers keep pointing to.
 *
 * @details Holds the handshake context shared by TX and RX of the link, and
 *   the RX decode buffers. The buffers are sized for the FULL view, which is
 *   the largest one, so they fit any scope (BASE and SYSTEM are prefixes of
 *   it). Temporary: to be replaced by buffers generated per channel set.
 */

struct ComLinkRuntime {
    CombusHandshakeContext handshake = {};
    uint16_t analogRx[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)]   = {};
    bool     digitalRx[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {};
};


/**
 * @brief The caller-owned arrays used by combus_links_init().
 *
 * @details All four arrays must have the same length, at least the number of
 *   links, and static storage duration.
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
 * @brief Validate one ComDevMap.
 *
 * @details Refused (logged, returns false): a null device, port or protocol;
 *   a BIDIRECTIONAL link not wired TX_RX, or a UNIDIRECTIONAL one wired TX_RX;
 *   an activation other than ALWAYS; a missing transmit rate on a transmitting
 *   end; a line rate above the port ceiling.
 *
 * @param map  Wiring to validate.
 * @return     true if the wiring is acceptable.
 */

bool combus_link_check( const ComDevMap& map );


/**
 * @brief Fill the ComBusLink descriptor of one wired link.
 *
 * @details Runs combus_link_check() first. The scope gives the layer and the
 *   frame size (channel counts of the matching view); the end gives which
 *   of TX and RX are configured. `link` is only written on success.
 *
 * @param map      Wiring of the link.
 * @param com      Transport already opened for map.port.
 * @param runtime  Storage of this link (handshake context, RX buffers).
 * @param link     Descriptor to fill.
 * @return         true on success.
 */

bool combus_link_build( const ComDevMap& map,
                        NodeCom*         com,
                        ComLinkRuntime&  runtime,
                        ComBusLink&      link );


/**
 * @brief Bring up every link of a ComDevMap table.
 *
 * @details For each map: validate, open the port, build the link. The TX and
 *   RX pools are registered first, then combus_protocol_init_all() runs. A map
 *   that fails is logged and left inactive; the others still start.
 *
 * @param maps     Table of wirings.
 * @param count    Number of entries in @p maps.
 * @param storage  Caller-owned arrays, at least @p count long.
 * @param pinReg   Optional pin registry used to claim the port pins.
 * @return         true if every link was brought up.
 */

bool combus_links_init( const ComDevMap*       maps,
                        uint8_t                count,
                        const ComLinksStorage& storage,
                        PinReg*                pinReg = nullptr );

// EOF combus_links.h
