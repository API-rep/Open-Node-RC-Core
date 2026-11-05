/******************************************************************************
 * @file  combus_tx.h
 * @brief ComBus transmitter: the bus to data frames and handshakes, for every link.
 *
 * @details Transport-independent: each link writes through its own NodeCom, at its
 *   own rate.  Non-blocking: a link does nothing until its period has elapsed.
 *
 *   The state of each link lives in a pool that the caller owns.  The usual entry
 *   point is combus_links_init(), which registers the pool and starts the links.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>   // CombusHandshakeContext



// =============================================================================
// 1. PER-LINK STATE
// =============================================================================

/**
 * @brief State of the transmission of one link.
 *
 * @details Exposed so that the caller can declare its static array without an
 *   internal header.  Filled by combus_tx_init(), updated by combus_tx_update().
 */

struct CombusTxState {
	NodeCom*       nodeCom  = nullptr;   ///< transport of the link
	ComBusFrameCfg frameCfg = {};        ///< capacity of the link: the clamp of the counts below

	uint8_t  seq      = 1u;              ///< rolling frame counter, 1..255; 0 is reserved (see combus_frame.h)
	uint32_t lastTxMs = 0u;              ///< millis() at the last data frame
	uint32_t periodMs = 0u;              ///< period of the data frames; 0 = link not started

		// Channels carried: the prefix of the view of the layer, or the table of the link
	uint8_t analogWireEnd  = 0u;         ///< number of analog channels to encode
	uint8_t digitalWireEnd = 0u;         ///< number of digital channels to encode

		// Contract of the link, shared with the RX side of the same link
	CombusHandshakeContext* handshakeCtx = nullptr;

	bool active = true;                  ///< false while a coupled link is detached: nothing is sent
};



// =============================================================================
// 2. POOL REGISTRATION
// =============================================================================

/**
 * @brief Register the pool of per-link states, once at boot.
 *
 * @details The module allocates nothing: the caller sizes the pool to its links.
 *   Must be called before the first combus_tx_init().  A second call is logged and
 *   ignored.
 *
 * @param buffer    Static array of states, owned by the caller.
 * @param capacity  Number of slots in @p buffer.
 */

void combus_tx_register_pool( CombusTxState* buffer, uint8_t capacity );



// =============================================================================
// 3. PER-LINK INIT
// =============================================================================

/**
 * @brief Start the transmission of one link, and arm its handshake burst.
 *
 * @details Does nothing if the pool is not registered, if @p linkIdx is out of range,
 *   if @p nodeCom is null or if @p txHz is 0.  A rate above 1000 Hz gives a period of 0
 *   and a silent link: combus_link_check() refuses it.
 *
 * @warning A @p layer other than BASE or SYSTEM is a configuration error: the system
 *   halts.
 *
 * @param linkIdx   Slot of this link in the pool.
 * @param nodeCom   Transport of the link.
 * @param frameCfg  Capacity of the link: the clamp of the channels sent.
 * @param txHz      Rate of the data frames, 1 to 1000 Hz.
 * @param layer     Layer of the link: its frames carry the prefix of this view, unless
 *                  the handshake context gives a table.
 */

void combus_tx_init( uint8_t        linkIdx,
                     NodeCom*       nodeCom,
                     ComBusFrameCfg frameCfg,
                     uint32_t       txHz,
                     ChanLayer      layer );



// =============================================================================
// 4. TRANSMIT UPDATE
// =============================================================================

/**
 * @brief Transmission of every link: the handshakes due, then a data frame if its time has come.
 *
 * @details Non-blocking, to call on every loop.  A bidirectional end sends no data
 *   while its contract is UNKNOWN.  A transmit-only end always sends.
 *
 * @param bus  The ComBus to send.  Nothing is sent if it is null.
 */

void combus_tx_update( const ComBus* bus );



// =============================================================================
// 5. CONTROL OF A LINK
// =============================================================================

/**
 * @brief Attach the handshake context of one link to its transmission.
 *
 * @details Called by combus_link_start(), before combus_tx_init(), with the same
 *   context as combus_rx_set_handshake_ctx().
 *
 * @param linkIdx  Slot of this link in the pool.
 * @param ctx      Handshake context of this link; nullptr: no handshake and no contract
 *                 (tests of the codec).
 */

void combus_tx_set_handshake_ctx( uint8_t linkIdx, CombusHandshakeContext* ctx );


/**
 * @brief Activate or suspend the transmission of one link, on a change of its coupling.
 *
 * @details Suspended: neither handshake nor data is sent.  Activated: the handshake
 *   starts again with a burst, as at boot.
 *
 * @param linkIdx  Slot of this link in the pool.
 * @param active   true to transmit, false to suspend.
 */

void combus_tx_set_active( uint8_t linkIdx, bool active );

// EOF combus_tx.h
