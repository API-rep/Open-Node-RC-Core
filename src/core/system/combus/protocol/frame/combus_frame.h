/******************************************************************************
 * @file combus_frame.h
 * ComBus frame codec - encode, decode, CRC-8, and bus application.
 *
 * @details Binary frame format (variable length):
 * @code
 *  Offset  Size  Field
 *  ------  ----  -----
 *   0       1    SOF          CombusFrameSof (0xAA)
 *   1-4     4    header       CombusFrameHeader fields (see outputs_struct.h)
 *   5       var  digital[]    bits packed LSB-first, ceil(n_digital/8) bytes
 *   5+d     var  analog[]     uint16_t LE, n_analog entries
 *   last    1    crc8         CRC-8/MAXIM over bytes [0 ... last-1]
 * @endcode
 *
 * RL2: header is now 4 bytes (was 5 before — runLevel removed from header,
 * travels as a standard combus channel RUNLEVEL, scope LOCAL).
 *
 * `seq` semantics:
 *   - control frames: rolling counter in 1..255 (wraps 255 -> 1, never 0)
 *   - seq == 0        : RESERVED for a future handshake / versioning frame
 *                      (generic to every ComBus participant — RF node or
 *                      serial board — NOT limited to the machine <-> remote
 *                      link).  See combus_handshake.h for the dedicated,
 *                      structurally separate decoder path that will pick
 *                      it up once the handshake / versioning mechanism is
 *                      implemented.
 *
 * Individual machine instance UID is delegated to the transport layer.
 * See CombusFrameHeader in outputs_struct.h for the full wire layout.

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
  /// RL2: header is now 4 bytes (was 5 before — runLevel removed).
static_assert(sizeof(CombusFrameSof) + sizeof(CombusFrameHeader) == 5u,
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
 * LY2 — the codec iterates only over `comBus.analogBus[0..analogWireEnd)`
 * and `comBus.digitalBus[0..digitalWireEnd)`.  These two counters are
 * resolved at TX init time from the link's `ChanLayer` (REMOTE → CH_COUNT
 * of the Remote view, LOCAL → CH_COUNT of the Local view, FULL → CH_COUNT
 * of the Full view).  This is the ONLY behavioural change vs. the
 * pre-LY2 codec — the wire payload is now sized to the layer's prefix
 * length instead of the full enum set.
 *
 * @param[in]  cfg             Static layout descriptor (nAnalog, nDigital).
 * @param[out] outputBuffer    Output buffer pointer (sized by the caller).
 * @param[in]  combus          Source ComBus instance to encode.
 * @param[in]  seq             Sequence counter (caller increments).
 * @param[in]  failSafe        Upstream failsafe flag.
 * @param[in]  analogWireEnd   LY2 — number of analog channels to encode
 *                             (resolved at TX init from link->layer).
 * @param[in]  digitalWireEnd  LY2 — number of digital channels to encode
 *                             (resolved at TX init from link->layer).
 *
 * @return Number of bytes written into outputBuffer, 0 on error.
 */

uint8_t combus_frame_encode( const ComBusFrameCfg& cfg,
                             uint8_t*              outputBuffer,
                             const ComBus*         combus,
                             uint8_t               seq,
                             bool                  failSafe,
                             uint8_t               analogWireEnd,
                             uint8_t               digitalWireEnd );


/**
 * @brief Decode a binary frame stored in the input buffer into a ComBusFrame.
 *
 * @details Validates SOF, frame length, and CRC before unpacking.
 * Rejects frames whose analog count exceeds cfg.nAnalog.
 * Digital bits exceeding cfg.nDigital are silently truncated.
 *
 * LY2 — decode is layer-agnostic.  It writes by index into the
 * caller-provided buffers without any notion of layer.  The layer
 * resolution happens at apply time (see combus_frame_apply).
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
 *  provided ComBus arrays. Caller must ensure array sizes match nAnalog
 *  and nDigital in the frame.
 *
 * @note Caller param pass the ChanLayer identity (typically REMOTE for any
 *  RX-only node) to enforce channel access security. If the caller layer
 *  does not match the channel's layer, the write will be silently rejected by
 *  combus_set_*() and the channel will not be updated.
 *
 * LY2 — apply is layer-agnostic.  It writes by index into the
 * caller-provided ComBus arrays without any notion of layer.  The
 * layer check is delegated to combus_set_*() which reads the
 * channel's declared `layer` from the shared backing storage.
 *
 * @param[in]  cfg         Static layout descriptor — clamp values come from cfg.nAnalog / nDigital.
 * @param[out] combus      Target ComBus to update.
 * @param[in]  inputFrame  Source frame (from combus_frame_decode).
 * @param[in]  caller      Propagation layer of the node calling this function.
 */

void combus_frame_apply( const ComBusFrameCfg& cfg,
                         ComBus*               combus,
                         const ComBusFrame*    inputFrame,
                         ChanLayer             caller );




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