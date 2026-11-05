/******************************************************************************
 * @file  combus_rx.h
 * @brief ComBus receiver: bytes of a transport to decoded frames, for every link.
 *
 * @details Transport-independent: each link reads through its own NodeCom.  The
 *   bytes pile up in a ring buffer, then go to the decoder of their kind of frame:
 *   data (frame/), handshake (handshake/) or coupling (coupling/).  A SOF scan finds
 *   the next frame after corrupted bytes.
 *
 *   The state of each link lives in a pool that the caller owns.  The usual entry
 *   point is combus_links_init(), which registers the pool and starts the links.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>   // CombusHandshakeContext



// =============================================================================
// 1. RING BUFFER CAPACITY
// =============================================================================

	/// Capacity of the ring buffer of one link: the length of a frame fits one byte, so a whole frame always fits.
static constexpr uint8_t CombusRxBufSize = UINT8_MAX;



// =============================================================================
// 2. PER-LINK STATE
// =============================================================================

/**
 * @brief Mode of the reception of one link.
 */

enum class CombusRxMode : uint8_t {
	OFF,       ///< coupled link detached: the bytes received are dropped
	PAIRING,   ///< coupled link pairing: coupling frames only
	ON         ///< every frame
};


/**
 * @brief State of the reception of one link.
 *
 * @details Exposed so that the caller can declare its static array without an
 *   internal header.  Filled by combus_rx_init(), updated by combus_rx_update().
 */

struct CombusRxState {
		// Transport and layout of the link
	NodeCom*       nodeCom  = nullptr;   ///< transport of the link
	ComBusFrameCfg frameCfg = {};        ///< capacity of the link: the largest frame accepted

		// Ring buffer, between the transport and the decoders
	uint8_t rxBuf[CombusRxBufSize] = {};
	uint8_t rxHead  = 0u;                ///< index of the oldest byte
	uint8_t rxCount = 0u;                ///< number of bytes held

		// Last data frame accepted; its analog and digital buffers belong to the caller
	ComBusFrame snap         = {};
	bool        snapValid    = false;    ///< snap holds a frame received under a valid contract
	uint32_t    lastRxMs     = 0u;       ///< millis() at the last frame accepted
	bool        everReceived = false;    ///< a frame was accepted since the link started

		// Contract of the link, shared with the TX side of the same link
	CombusHandshakeContext* handshakeCtx = nullptr;

		// Where the frames accepted are written; nullptr: nowhere (tests of the codec)
	ComBus* target = nullptr;

	ChanLayer    writeLayer    = ChanLayer::UNDEFINED;   ///< layer of the link: the identity of the writes to `target`
	uint32_t     linkTimeoutMs = 0u;                     ///< silence before the link is lost; 0: see peer_or
	CombusRxMode mode          = CombusRxMode::ON;       ///< OFF and PAIRING: see CombusRxMode
};


	/// "No link" value for the excluded link of combus_rx_peer_or().
static constexpr uint8_t CombusRxNoLink = 0xFFu;



// =============================================================================
// 3. POOL REGISTRATION
// =============================================================================

/**
 * @brief Register the pool of per-link states, once at boot.
 *
 * @details The module allocates nothing: the caller sizes the pool to its links.
 *   Must be called before the first combus_rx_init().  A second call is logged and
 *   ignored.
 *
 * @param buffer    Static array of states, owned by the caller.
 * @param capacity  Number of slots in @p buffer.
 */

void combus_rx_register_pool( CombusRxState* buffer, uint8_t capacity );



// =============================================================================
// 4. PER-LINK INIT
// =============================================================================

/**
 * @brief Start the reception of one link, with an empty ring buffer and an UNKNOWN contract.
 *
 * @details Does nothing if the pool is not registered, if @p linkIdx is out of range
 *   or if a pointer is null.  The module does not know the size of the buffers: their
 *   capacity is part of the contract with the caller.
 *
 * @warning With a @p target, a @p layer other than BASE or SYSTEM is a configuration
 *   error: the system halts.
 *
 * @param linkIdx     Slot of this link in the pool.
 * @param nodeCom     Transport of the link.
 * @param frameCfg    Capacity of the link: a larger frame is refused.
 * @param analogBuf   Output buffer owned by the caller, at least `frameCfg.nAnalog` entries.
 * @param digitalBuf  Output buffer owned by the caller, at least `frameCfg.nDigital` entries.
 * @param target      ComBus that receives the frames accepted.  nullptr: they stay in the
 *                    snapshot only, which the tests of the codec use.
 * @param layer       Layer of the link, the identity of the writes: a BASE link writes
 *                    BASE channels only, a SYSTEM link BASE and SYSTEM channels.
 */

void combus_rx_init( uint8_t        linkIdx,
                     NodeCom*       nodeCom,
                     ComBusFrameCfg frameCfg,
                     uint16_t*      analogBuf,
                     bool*          digitalBuf,
                     ComBus*        target = nullptr,
                     ChanLayer      layer  = ChanLayer::UNDEFINED );



// =============================================================================
// 5. RECEIVE UPDATE
// =============================================================================

/**
 * @brief Reception of every link: read the transport, decode, watch the contract.
 *
 * @details Non-blocking, to call on every loop.  On each link, reads all the bytes
 *   available, then decodes every complete frame.  A STABLE contract drops back to
 *   UNKNOWN after a silence longer than the link timeout, once data has flowed, or
 *   after kCombusHandshakeAckTimeoutMs without ACK on a bidirectional end.
 */

void combus_rx_update( void );



// =============================================================================
// 6. STATE OF A LINK
// =============================================================================

/**
 * @brief Last data frame accepted on one link.
 *
 * @details The firmware reads the bus, or combus_rx_peer_or() for a both_or channel:
 *   this access is how the tests observe what was received.
 *
 * @param linkIdx  Slot of this link in the pool.
 *
 * @return The snapshot, valid until the next frame; nullptr if there is none or if
 *   @p linkIdx is out of range.
 */

const ComBusFrame* combus_rx_snapshot( uint8_t linkIdx );


/**
 * @brief Milliseconds elapsed since the last frame accepted on one link.
 *
 * @param linkIdx  Slot of this link in the pool.
 *
 * @return The age of the frame; UINT32_MAX if none was accepted, or if @p linkIdx is
 *   out of range.
 */

uint32_t combus_rx_age_ms( uint8_t linkIdx );


/**
 * @brief True if one link has a valid contract and has received a frame recently.
 *
 * @param linkIdx    Slot of this link in the pool.
 * @param timeoutMs  Maximum age of the last frame accepted (ms).
 *
 * @return true if a frame arrived within @p timeoutMs under a valid contract; false
 *   otherwise, or if @p linkIdx is out of range.  A link without a handshake context
 *   has no contract to check.
 */

bool combus_rx_is_alive( uint8_t linkIdx, uint32_t timeoutMs );


/**
 * @brief Set the silence after which a receiving link counts as lost.
 *
 * @details Used by the watch of the contract and by combus_rx_peer_or().  Set by
 *   combus_links_watch().
 *
 * @param linkIdx    Slot of this link in the pool.
 * @param timeoutMs  Silence (ms) after which the link is lost; 0 = never alive for
 *                   combus_rx_peer_or().
 */

void combus_rx_set_link_timeout( uint8_t linkIdx, uint32_t timeoutMs );


/**
 * @brief Set the mode of the reception of one link, on a change of its coupling.
 *
 * @details Leaving ON, the link is no longer alive and its contract drops to UNKNOWN.
 *   The last frame is forgotten, so that a link back ON starts as never seen.
 *
 * @param linkIdx  Slot of this link in the pool.
 * @param mode     The new mode.
 */

void combus_rx_set_mode( uint8_t linkIdx, CombusRxMode mode );


/**
 * @brief OR of one digital channel over the values received from the live links.
 *
 * @details The both_or merge (anti ping-pong, see both_or.md).  Only the links that
 *   are alive, with their own timeout, and whose last frame carried the channel count.
 *   @p excludeLink is left out: the value sent on a link never includes what that link
 *   sent.
 *
 * @param ch           Digital channel ID.
 * @param excludeLink  Link to leave out, or CombusRxNoLink.
 *
 * @return true if a live link, other than @p excludeLink, received true.
 */

bool combus_rx_peer_or( uint8_t ch, uint8_t excludeLink );



// =============================================================================
// 7. HANDSHAKE CONTEXT
// =============================================================================

/**
 * @brief Attach the handshake context of one link to its reception.
 *
 * @details Called by combus_protocol_init(), before combus_rx_init(), with the same
 *   context as combus_tx_set_handshake_ctx().
 *
 * @param linkIdx  Slot of this link in the pool.
 * @param ctx      Handshake context of this link; nullptr: the link has no contract and
 *                 its frames are always accepted (tests of the codec).
 */

void combus_rx_set_handshake_ctx( uint8_t linkIdx, CombusHandshakeContext* ctx );

// EOF combus_rx.h
