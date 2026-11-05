/*!****************************************************************************
 * @file  combus_coupling.h
 * @brief ComBus coupling frame: the frame the pairing speaks.
 *
 * @details The coupling frame is apart from the handshake: it carries the identity of
 *   a node, the age of its hitch and its functions, and serves only the pairing of a
 *   coupled link (ON_COUPLING).  It neither makes a link alive nor validates a
 *   contract.
 *
 *   Wire slot: `seq == 0`, as the handshake, with the header flag
 *   kCombusFrameFlagCoupling.  combus_rx routes it here, and every other `seq == 0`
 *   frame to the handshake.  A firmware without the coupling frame would read it as a
 *   handshake of the wrong length: its CRC check fails, the frame is dropped and the
 *   stream re-syncs.
 *
 *   Payload layout (kCombusCouplingPayloadLen = 13), integers little-endian:
 *     [0]       msg           CouplingMsg (request or answer)
 *     [1]       round         round of the pairing (1..N)
 *     [2]       addr.type     LinkAddrType
 *     [3..8]    addr.bytes    the address (MAC48: six bytes)
 *     [9..10]   hitchAgeMs    ms since the hitch trigger rose, saturated at 65535
 *     [11..12]  features      mask of the functions (reserved: 0 until defined)
 *
 *   A received frame is handed to the handler registered by the watch of the links,
 *   which passes it to the pairing: this module does not depend on the layers above
 *   it.  The last frame of the first kCombusCouplingMaxLinks links is also recorded,
 *   for the tests and the diagnostics.
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/frame/combus_frame.h>           // CombusFrameHeaderLen
#include <core/system/combus/protocol/link/combus_peer_link_defs.h>   // NodeAddr



// =============================================================================
// 1. WIRE CONSTANTS
// =============================================================================

/// Bit of the header `flags` byte that marks a `seq == 0` frame as a coupling frame.
static constexpr uint8_t kCombusFrameFlagCoupling = 0x04u;

/// Fixed payload length of the coupling frame, in bytes.
static constexpr uint8_t kCombusCouplingPayloadLen = 13u;

/// Total coupling frame size on the wire (SOF + header + payload + CRC).
static constexpr uint8_t kCombusCouplingFrameLen =
	CombusFrameHeaderLen + kCombusCouplingPayloadLen + sizeof(uint8_t);

/// Links whose last received coupling frame is recorded.
static constexpr uint8_t kCombusCouplingMaxLinks = 4u;



// =============================================================================
// 2. FRAME CONTENT
// =============================================================================

/**
 * @brief Kind of a coupling frame.
 */

enum class CouplingMsg : uint8_t {
	REQUEST = 1u,   ///< sent by the socket, one per round
	ANSWER  = 2u    ///< sent by a hitched plug, not yet paired
};


/**
 * @brief Content of a coupling frame.
 */

struct CouplingFrame {
	CouplingMsg msg        = CouplingMsg::REQUEST;
	uint8_t     round      = 0u;    ///< round of the pairing (1..N)
	NodeAddr    addr       = {};    ///< identity of the sender
	uint16_t    hitchAgeMs = 0u;    ///< ms since the hitch trigger rose, saturated at 65535
	uint16_t    features   = 0u;    ///< mask of the functions (reserved: 0 until defined)
};



// =============================================================================
// 3. ENCODE AND DECODE
// =============================================================================

/// Age of a hitch on the wire: @p ms saturated at 65535.
uint16_t combus_coupling_hitch_age( uint32_t ms );


/**
 * @brief Encode a coupling frame.
 *
 * @param buf     Output buffer.
 * @param bufLen  Its size.
 * @param frame   Content to encode.
 *
 * @return Bytes written (kCombusCouplingFrameLen), or 0 if @p buf is null or too small.
 */

uint8_t combus_coupling_encode( uint8_t* buf, uint8_t bufLen, const CouplingFrame& frame );


/**
 * @brief Decode a coupling frame from a linear buffer.
 *
 * @param buf  Bytes of the frame, SOF first.
 * @param len  Their number.
 * @param out  Content decoded; written on success only.
 *
 * @return true if @p len is kCombusCouplingFrameLen, the CRC matches, the header is
 *         the one of a coupling frame and the message is known.
 */

bool combus_coupling_decode( const uint8_t* buf, uint8_t len, CouplingFrame& out );


/**
 * @brief Try to decode a coupling frame at the head of a ring buffer, for combus_rx.
 *
 * @details Same contract as combus_handshake_tryDecode(): 0 while the frame is
 *   incomplete; on a refused frame, the SOF is dropped (1 byte) and 0 returned; on
 *   success, the frame is consumed, recorded, handed to the handler, and its length
 *   returned.
 *
 * @param linkIdx      Index of the link the bytes come from.
 * @param ringBuf      Ring buffer base pointer.
 * @param ringBufSize  Ring buffer capacity.
 * @param ringHead     Index of the oldest byte; moved by what is consumed.
 * @param ringCount    Number of bytes held; lowered by what is consumed.
 *
 * @return Length of the frame consumed, or 0.
 */

uint8_t combus_coupling_tryDecode( uint8_t  linkIdx,
                                   uint8_t* ringBuf,
                                   uint8_t  ringBufSize,
                                   uint8_t& ringHead,
                                   uint8_t& ringCount );



// =============================================================================
// 4. IDENTITY, RECORDS AND HANDLER
// =============================================================================

/// Identity of this node: the factory MAC of the ESP32, type NONE if it cannot be read.
NodeAddr combus_coupling_local_addr( void );


/// Number of coupling frames received on @p linkIdx since the last reset; 0 beyond the recorded links.
uint32_t combus_coupling_rx_count( uint8_t linkIdx );


/**
 * @brief Last coupling frame received on a link.
 *
 * @param linkIdx  Index of the link.
 * @param out      The frame; written when there is one.
 *
 * @return false if none was received since the last reset, or beyond the recorded links.
 */

bool combus_coupling_last_rx( uint8_t linkIdx, CouplingFrame& out );


/// Forget the received coupling frames (tests, re-init).
void combus_coupling_reset( void );


/// Handler of a received coupling frame.
using CouplingRxHandler = void (*)( uint8_t linkIdx, const CouplingFrame& frame );


/// Register the handler of the received coupling frames (nullptr: none).
void combus_coupling_set_rx_handler( CouplingRxHandler handler );

// EOF combus_coupling.h
