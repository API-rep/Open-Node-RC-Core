/******************************************************************************
 * @file  combus_frame.h
 * @brief ComBus control frame codec: encode, decode, CRC-8 and bus application.
 *
 * @details Binary frame format (variable length):
 * @code
 *  Offset  Size  Field
 *  ------  ----  -----
 *   0       1    SOF          CombusFrameSof (0xAA)
 *   1-4     4    header       CombusFrameHeader (combus_frame_defs.h)
 *   5       var  digital[]    bits packed LSB-first, ceil(n_digital/8) bytes
 *   5+d     var  analog[]     uint16_t LE, n_analog entries
 *   last    1    crc8         CRC-8/MAXIM over bytes [0 ... last-1]
 * @endcode
 *
 *   `seq` is a rolling counter in 1..255 (wraps 255 -> 1, never 0).  `seq == 0` is
 *   reserved on every transport: the handshake (combus_handshake.h) and the
 *   coupling frame (combus_coupling.h) travel there, on their own decoder paths.
 *
 *   The identity of a machine instance is delegated to the transport layer.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/combus_defs.h>
#include <core/system/combus/protocol/frame/combus_frame_defs.h>



// =============================================================================
// 1. CONSTANTS
// =============================================================================

	/// Start-of-frame sentinel byte.
static constexpr uint8_t CombusFrameSof = 0xAAu;

	/// Guard: catch any unexpected padding introduced in CombusFrameHeader.
static_assert(sizeof(CombusFrameSof) + sizeof(CombusFrameHeader) == 5u,
              "Bad CombusFrameHeader size detected: check for unexpected padding");

	/// Fixed combus header size in bytes
static constexpr uint8_t CombusFrameHeaderLen = sizeof(CombusFrameSof) + sizeof(CombusFrameHeader);

	/// Minimum valid frame size (fixed header + CRC byte).
static constexpr uint8_t CombusFrameMinLen = CombusFrameHeaderLen + sizeof(uint8_t);



// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Encode a ComBus state into a binary control frame.
 *
 * @details The frame carries the first channels of the bus (the prefix of the
 *   view of a scope), or the channels of a table on a link between nodes.  The
 *   failsafe state travels as a channel (FAILSAFE_HARD), not as a header flag.
 *
 * @param[in]  cfg             Static layout descriptor: the clamp of the counts below.
 * @param[out] outputBuffer    Output buffer, sized by the caller to at least
 *                             CombusFrameHeaderLen + ceil(nDigital/8) + nAnalog*2 + 1 bytes.
 * @param[in]  combus          Source ComBus instance.
 * @param[in]  seq             Sequence counter, 1..255 (the caller increments it).
 *                             0 is refused: reserved, see the file banner.
 * @param[in]  analogWireEnd   Number of analog channels to encode, set per link at TX init.
 * @param[in]  digitalWireEnd  Number of digital channels to encode, set per link at TX init.
 * @param[in]  digitalValues   Digital values to send instead of the bus values, indexed
 *                             by wire position (at least @p digitalWireEnd entries): the
 *                             values prepared for one link (both_or without echo).
 *                             nullptr = bus values.
 * @param[in]  analogMap       Link between nodes: wire position -> analog channel ID
 *                             (ComPeerLink::analog.ids).  nullptr = the wire position
 *                             is the channel ID.
 * @param[in]  digitalMap      Same for the digital channels (ComPeerLink::digital.ids).
 *
 * @return Number of bytes written; 0 if a pointer is null, `seq` is 0, or the frame
 *   would not fit a one-byte length (255 bytes).
 */

uint8_t combus_frame_encode( const ComBusFrameCfg& cfg,
                             uint8_t*              outputBuffer,
                             const ComBus*         combus,
                             uint8_t               seq,
                             uint8_t               analogWireEnd,
                             uint8_t               digitalWireEnd,
                             const bool*           digitalValues = nullptr,
                             const uint8_t*        analogMap     = nullptr,
                             const uint8_t*        digitalMap    = nullptr );


/**
 * @brief Decode and validate a binary control frame.
 *
 * @details Validates SOF, length and CRC before unpacking.  Refuses a frame with more
 *   analog or digital channels than `cfg`.  Only the nDigital bits announced are
 *   unpacked: the padding bits of the last byte never reach the buffer.  The result
 *   is indexed by wire position, with no notion of scope.
 *
 * @param[in]  cfg          Static layout descriptor: the capacities of the output buffers.
 * @param[out] outputFrame  Output frame; its analog and digital pointers are set by the caller.
 * @param[in]  inputBuffer  Raw received bytes.
 * @param[in]  len          Number of bytes available in inputBuffer.
 *
 * @return true if the frame is valid and outputFrame was fully populated, false otherwise.
 */

bool combus_frame_decode( const ComBusFrameCfg& cfg,
                          ComBusFrame*          outputFrame,
                          const uint8_t*        inputBuffer,
                          uint8_t               len );


/**
 * @brief Apply a decoded frame onto a live ComBus.
 *
 * @details Stamps `combus->lastFrameMs`, then writes the channels this node receives:
 *   the direction of the channel decides without a table, the table of the link
 *   with one.  A channel this node sends is never overwritten by its peer, and a
 *   both_or channel is never written (combus_rx merges it, see protocol/both_or.md).
 *   Channels beyond the effective count stay untouched.
 *
 * @note Writes go through combus_set_*(): a channel whose scope does not match
 *   `caller` is silently refused there.
 *
 * @param[in]  cfg         Static layout descriptor: the clamp of the channel counts.
 * @param[out] combus      Target ComBus; its arrays hold at least `cfg` entries.
 * @param[in]  inputFrame  Source frame, from combus_frame_decode().
 * @param[in]  caller      Scope of the link (BASE between nodes, SYSTEM between boards).
 * @param[in]  peer        Link between nodes: maps a wire position to a channel and says
 *                         whether this node receives it.  nullptr = wire position =
 *                         channel ID.
 */

void combus_frame_apply( const ComBusFrameCfg& cfg,
                         ComBus*               combus,
                         const ComBusFrame*    inputFrame,
                         ChanLayer             caller,
                         const ComPeerLink*    peer = nullptr );


/**
 * @brief Compute CRC-8/MAXIM (Dallas 1-Wire) over a byte buffer.
 *
 * @param[in] data  Input bytes.
 * @param[in] len   Number of bytes.
 *
 * @return The CRC-8 value.
 */

uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len);

// EOF combus_frame.h
