/******************************************************************************
 * @file combus_frame.h
 * ComBus frame codec - encode, decode, CRC-8, and bus application.
 *
 * @details Binary frame format (variable length):
 * @code
 *  Offset  Size  Field
 *  ------  ----  -----
 *   0       1    SOF          CombusFrameSof (0xAA)
 *   1-6     6    header       CombusFrameHeader fields (see outputs_struct.h)
 *   7       var  digital[]    bits packed LSB-first, ceil(n_digital/8) bytes
 *   7+d     var  analog[]     uint16_t LE, n_analog entries
 *   last    1    crc8         CRC-8/MAXIM over bytes [0 ... last-1]
 * @endcode
 *
 * Individual machine instance ( UID) is delegated to the transport layer.
 * See CombusFrameHeader in outputs_struct.h for the full wire layout.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <struct/combus_struct.h>
#include <struct/outputs_struct.h>


// =============================================================================
// 1. CONSTANTS
// =============================================================================

  /// Start-of-frame sentinel byte.
static constexpr uint8_t CombusFrameSof = 0xAAu;

  /// Guard: catch any unexpected padding introduced in CombusFrameHeader.
static_assert(sizeof(CombusFrameSof) + sizeof(CombusFrameHeader) == 7u,
              "Bad CombusFrameHeader size detected: check for unexpected padding");

  /// Fixed combus header size in bytes
static constexpr uint8_t CombusFrameHeaderLen = sizeof(CombusFrameSof) + sizeof(CombusFrameHeader);

  /// Minimum valid frame size (fixed header + CRC byte).
static constexpr uint8_t CombusFrameMinLen = CombusFrameHeaderLen + sizeof(uint8_t);



// =============================================================================
// 2. FLAGS BITMASK - FRAME FLAGS BIT POSITIONS
// =============================================================================

  // Only transport-level status lives here; do not overload with application-level state.

#define COMBUS_FLAG_FAILSAFE     (1u << 0)  ///< upstream failsafe active (link-loss or remote disconnect)



// =============================================================================
// 3. PUBLIC API
// =============================================================================

/**
 * @brief Encode a ComBus state into a binary frame stored in the buffer.
 *
 * @details Serializes runLevel, flags, all analog and digital channels into
 * the compact frame format with an automatic CRC8 check.
 *
 * Returns 0 if buf/bus are null or if the computed frame length would
 * overflow a uint8_t (i.e. caller requested more data than the protocol
 * can address with a single-byte length field).
 *
 * @param[in]  cfg           Static layout descriptor (envId, nAnalog, nDigital).
 * @param[out] outputBuffer  Output buffer pointer (sized by the caller).
 * @param[in]  combus        Source ComBus instance to encode.
 * @param[in]  seq           Sequence counter (caller increments).
 * @param[in]  failSafe      Upstream failsafe flag.
 *
 * @return Number of bytes written into outputBuffer, 0 on error.
 */

uint8_t combus_frame_encode( const ComBusFrameCfg& cfg,
                             uint8_t*              outputBuffer,
                             const ComBus*         combus,
                             uint8_t               seq,
                             bool                  failSafe );


/**
 * @brief Decode a binary frame stored in the input buffer into a ComBusFrame.
 *
 * @details Validates SOF, frame length, and CRC before unpacking.
 * Rejects frames whose analog count exceeds cfg.nAnalog.
 * Digital bits exceeding cfg.nDigital are silently truncated.
 *
 * @param[in]  cfg          Static layout descriptor — buffer capacities come from cfg.nAnalog / nDigital.
 * @param[out] outputFrame  Output frame — analog/digital pointers must be pre-set by the caller.
 * @param[in]  inputBuffer  Input buffer.
 * @param[in]  len          Number of bytes available in inputBuffer.
 *
 * @return true if frame is valid and was populated, false otherwise.
 */

bool combus_frame_decode( const ComBusFrameCfg& cfg,
                          ComBusFrame*          outputFrame,
                          const uint8_t*        inputBuffer,
                          uint8_t               len );




/**
 * @brief Apply a decoded frame onto a live ComBus.
 *
 * @details Writes runLevel, flags, analog and digital values into the
 * provided ComBus arrays. Caller must ensure array sizes match nAnalog
 * and nDigital in the frame.
 *
 * @param[in]  cfg         Static layout descriptor — clamp values come from cfg.nAnalog / nDigital.
 * @param[out] combus      Target ComBus to update.
 * @param[in]  inputFrame  Source frame (from combus_frame_decode).
 */

void combus_frame_apply( const ComBusFrameCfg& cfg,
                         ComBus*               combus,
                         const ComBusFrame*    inputFrame );




/**
 * @brief Compute CRC-8/MAXIM over a byte buffer.
 *
 * @param[in] data  Input bytes.
 * @param[in] len   Number of bytes.
 * 
 * @return CRC8 value.
 */

uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len);

// EOF combus_frame.h
