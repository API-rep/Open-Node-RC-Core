/******************************************************************************
 * @file  combus_handshake.cpp
 * @brief ComBus handshake frame dispatcher — stub implementation.
 *
 * @details Implements the structurally separate decoder path used when a
 *   frame's `seq` byte is `0u`.  See combus_handshake.h for the design
 *   contract.
 *
 *   Scope of THIS revision:
 *     - validate the structural shape of the frame (CRC-8, fixed length);
 *     - consume the bytes from the ring buffer to keep the link re-synced;
 *     - record liveness so the future versioning layer can poll it.
 *
 *   Scope EXPLICITLY out of this revision (to be added later):
 *     - payload interpretation (MD5, sender address, friend-cache update);
 *     - handshake transmission path (TX side);
 *     - high-priority QoS routing.
 *****************************************************************************/

#include "combus_handshake.h"

#include <string.h>
#include <stddef.h>

#include <core/system/combus/combus_frame.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/**
 * @brief Persistent handshake-liveness flag.
 *
 * @details Tracked separately from the control-frame `everReceived` so the
 *   future versioning layer can tell control traffic from handshake traffic
 *   without inspecting the snapshot itself.
 *
 *   Lifetime: static — valid for the entire program run.
 */
static bool s_handshakeEverReceived = false;


// =============================================================================
// 2. RX DISPATCHER — HANDLER STUB
// =============================================================================

/**
 * @brief Read the byte at logical ring index `i` (0 = oldest).
 *
 * @details Local helper duplicated from combus_rx.cpp (intentional — the
 *   handshake module owns its decoder, no dependency on RX internals).
 */
static uint8_t ringByteAt(const uint8_t* ringBuf,
                          uint8_t        ringBufSize,
                          uint8_t        ringHead,
                          uint8_t        i)
{
    return ringBuf[(uint8_t)(ringHead + i) % ringBufSize];
}


// ----------------------------------------------------------------------------

uint8_t combus_handshake_tryDecode(
    uint8_t*       ringBuf,
    uint8_t        ringBufSize,
    uint8_t&       ringHead,
    uint8_t&       ringCount )
{
    if (!ringBuf || ringBufSize == 0u) { return 0u; }
    if (ringCount == 0u)               { return 0u; }

    // --- 1. Compute expected wire length for a handshake frame ---
    // Handshake frames have a fixed payload length of kCombusHandshakePayloadLen
    // (currently zero — see combus_handshake.h).  The header and CRC-8 are
    // mandatory, exactly as for a control frame.
    const uint8_t expectedLen = sizeof(CombusFrameSof)
                             + sizeof(CombusFrameHeader)
                             + kCombusHandshakePayloadLen
                             + sizeof(uint8_t);

    if (ringCount < expectedLen) {
        // Not enough bytes yet — caller will retry on the next poll.
        return 0u;
    }

    // --- 2. Copy candidate bytes to a linear scratch buffer ---
    // We cannot CRC-validate a ring buffer in place without unrolling the
    // modulo, so we linearise once.  Allocates sizeof(expectedLen) on the
    // stack; safe for any legal ComBus frame size (≤ 255).
    uint8_t linear[ sizeof(CombusFrameSof)
                  + sizeof(CombusFrameHeader)
                  + 255u              // upper bound on handshake payload
                  + sizeof(uint8_t) ];

    for (uint8_t i = 0u; i < expectedLen; ++i) {
        linear[i] = ringByteAt(ringBuf, ringBufSize, ringHead, i);
    }

    // --- 3. Validate CRC-8/MAXIM over the header + payload portion ---
    uint8_t crcExpected = linear[expectedLen - 1u];
    uint8_t crcActual   = combus_frame_crc8(linear, (uint8_t)(expectedLen - 1u));
    if (crcActual != crcExpected) {
        // CRC mismatch — the caller is responsible for re-syncing on SOF;
        // we deliberately do NOT consume here because the next byte might
        // also be a valid SOF for a fresh handshake attempt.
        sys_log_info("[COMBUS_HANDSHAKE] CRC mismatch — dropping %u bytes\n",
                     (unsigned)expectedLen);
        return 0u;
    }

    // --- 4. CRC OK — consume the validated frame from the ring buffer ---
    ringHead  = (uint8_t)(ringHead + expectedLen) % ringBufSize;
    ringCount = (uint8_t)(ringCount - expectedLen);

    s_handshakeEverReceived = true;

    // --- 5. Stub: payload interpretation is OUT OF SCOPE for this revision.
    //     The future versioning layer will hook here to parse MD5 + sender
    //     address, update the friend-cache, etc.  For now, log and move on.
    sys_log_info("[COMBUS_HANDSHAKE] valid frame received (%u bytes) — payload "
                 "interpretation deferred to a future revision\n",
                 (unsigned)expectedLen);

    return expectedLen;
}


// =============================================================================
// 3. PUBLIC STATUS HELPERS
// =============================================================================

bool combus_handshake_ever_received() {
    return s_handshakeEverReceived;
}

// EOF combus_handshake.cpp
