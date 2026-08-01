/******************************************************************************
 * @file  combus_handshake.h
 * @brief ComBus handshake frame handling — RESERVED wire slot, NOT YET
 *        implemented on the wire.
 *
 * @details The ComBus rolling sequence counter (`seq` field in
 *   `CombusFrameHeader`) is reserved to the range **1..255** for normal
 *   control frames.  The value **`seq == 0` is RESERVED** for a future
 *   handshake / versioning frame that will:
 *     - detect accidental version / layout mismatches between any two
 *       ComBus participants (machine <-> remote over RF, or mainboard
 *       <-> extension board over serial);
 *     - live on the high-priority QoS channel once that layer exists.
 *
 *   This header declares the dedicated dispatcher entry point used by the
 *   RX decoder.  It is **structurally separate** from the regular control-
 *   frame decode path so that the future handshake implementation can be
 *   added here without any refactor of `combus_frame.{h,cpp}`.
 *
 *   @note The handshake itself is NOT implemented in this revision.  The
 *     stub below only validates the structural shape of the frame (CRC,
 *     fixed payload length) and consumes the bytes from the RX ring buffer
 *     so the link stays re-synchronised.  No payload interpretation, no
 *     friend-cache update, no MD5 check — out of scope.
 *
 *   Generic to every ComBus participant (RF node or serial board), NOT
 *   limited to the machine <-> remote link.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/combus_frame.h>   // CombusFrameHeader, CombusFrameSof


// =============================================================================
// 1. WIRE CONSTANTS — HANDSHAKE FRAME
// =============================================================================

/**
 * @brief Start-of-frame sentinel byte for a handshake frame.
 *
 * @details Currently identical to the control-frame SOF (`CombusFrameSof`)
 *   — the discrimination is done on the `seq` byte that immediately follows
 *   the fixed 3-byte `ComBusFrameCfg` (nAnalog, nDigital, seq).
 *
 *   When the handshake gets its own dedicated SOF in the future (e.g. to
 *   allow coexistence with discovery / scan traffic on a shared bus), this
 *   constant is the natural place to change.
 */
static constexpr uint8_t CombusHandshakeSof = CombusFrameSof;  ///< placeholder

/**
 * @brief Fixed payload length of the handshake frame, in bytes.
 *
 * @details Excludes SOF, header and CRC-8.  Today: zero — the stub does
 *   not interpret any payload.  Will grow once the handshake protocol
 *   (MD5 of combus layout + version, sender address, …) is defined.
 *
 *   Total wire bytes for a handshake frame = 1 (SOF) +
 *   `CombusFrameHeaderLen` + `kCombusHandshakePayloadLen` + 1 (CRC-8).
 */
static constexpr uint8_t kCombusHandshakePayloadLen = 0u;

/// Total handshake frame size on the wire (SOF + header + payload + CRC).
static constexpr uint8_t CombusFrameHandshakeMinLen =
    sizeof(CombusFrameSof) + sizeof(CombusFrameHeader) +
    kCombusHandshakePayloadLen + sizeof(uint8_t);


// =============================================================================
// 2. RX DISPATCHER ENTRY POINT
// =============================================================================

/**
 * @brief Attempt to decode a complete handshake frame from the RX ring buffer.
 *
 * @details Called by `combus_rx.cpp::tryDecode()` once an SOF sentinel has
 *   been located AND the `seq` byte read at the right offset is `0u`.  This
 *   guarantees that a normal control frame (seq in 1..255) never reaches
 *   this function.
 *
 *   The current stub:
 *     1. Reads the full `CombusFrameHeader` from the ring buffer (skipping
 *        the SOF byte) into a local copy.
 *     2. Computes the expected wire length from
 *        `CombusFrameHeaderLen + kCombusHandshakePayloadLen + 1`.
 *     3. Waits for enough bytes in the ring buffer; if not enough, returns
 *        0 (caller will retry on the next poll).
 *     4. Copies the candidate frame, validates CRC-8/MAXIM, and consumes
 *        the bytes from the ring buffer on success.
 *     5. On a valid handshake, performs NO payload interpretation
 *        (handshake logic is out of scope of this revision).
 *
 *   @param[in,out] ringBuf      Pointer to the RX ring buffer (caller-
 *                               owned storage; opaque to this module).
 *   @param[in,out] ringHead     Read index — advanced on successful consume.
 *   @param[in,out] ringCount    Bytes currently held in the ring buffer —
 *                               decremented on successful consume.
 *
 *   @return Number of bytes consumed on a valid handshake frame,
 *           0 if no complete valid frame is currently available.
 *
 *   @note Implementer contract: this function MUST consume exactly the
 *     bytes it returns, and MUST leave `ringBuf` in a consistent state on
 *     every return path (success, not-yet-complete, or CRC failure).
 *     The caller does NOT decode the frame further — handshake
 *     interpretation lives behind this entry point.
 *
 *   @warning Side effects: bumps `lastRxMs` and `everReceived` on the
 *     shared `comBusRx` state via the supplied context.  Implemented
 *     separately from the control-frame path so the future handshake
 *     logic can attach its own metrics without disturbing control-frame
 *     accounting.
 */
uint8_t combus_handshake_tryDecode(
    uint8_t*       ringBuf,
    uint8_t        ringBufSize,
    uint8_t&       ringHead,
    uint8_t&       ringCount );


// =============================================================================
// 3. PUBLIC STATUS HELPERS
// =============================================================================

/**
 * @brief True if at least one valid handshake frame has ever been observed
 *   since boot, on any ComBus transport the node participates in.
 *
 * @details Mirrors `combus_rx_ever_received()` for control frames.  Exposed
 *   as a thin accessor so the future cache / versioning layer can poll
 *   liveness without including the full RX module.
 */
bool combus_handshake_ever_received();

// EOF combus_handshake.h
