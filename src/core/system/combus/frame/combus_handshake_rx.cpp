/******************************************************************************
 * @file  combus_handshake_rx.cpp
 * @brief RX side — decode a handshake frame from the ring buffer, compare
 *        the MD5 + version against the locally-generated copy, log one
 *        compact line per event.
 *
 * @details Format-md5 helper (`combus_handshake_formatMd5Hex`) is owned by
 *   combus_handshake.cpp.  Boot banner is also owned by the umbrella and
 *   invoked on the first decode attempt.
 *
 *   Scope EXPLICITLY out of this revision (to be added later):
 *     - friend-cache update ("amies" / sender address);
 *     - automatic burst at boot;
 *     - high-priority QoS routing.
 *****************************************************************************/

#include "combus_handshake_rx.h"

#include <string.h>
#include <stddef.h>

#include <core/system/combus/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. RX INTERNAL — RING READ HELPER
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


// =============================================================================
// 2. RX INTERNAL — MD5 + VERSION COMPARE + LOG
// =============================================================================

void combus_handshake_compareAndLog(
    const uint8_t* wireMd5,
    uint8_t        wireMajor,
    uint8_t        wireMinor )
{
    char wireMd5Hex[33];
    combus_handshake_formatMd5Hex(wireMd5, wireMd5Hex);

    // Bypass path — log "match" regardless of actual bytes, but still log
    // the on-wire payload so the bypass is auditable.
    if (COMBUS_MD5_CHECK_DISABLE) {
        sys_log_info(
            "[COMBUS_HANDSHAKE] MATCH(bypass)  wire md5=%s  ver=%u.%u\n",
            wireMd5Hex, (unsigned)wireMajor, (unsigned)wireMinor);
        return;
    }

    const bool md5Match = (memcmp(wireMd5,
                                 combus::wire::kCombusWireMd5, 16) == 0);
    const bool verMatch = (wireMajor == combus::wire::kProjectVersionMajor)
                       && (wireMinor == combus::wire::kProjectVersionMinor);


    if (md5Match && verMatch) {
        sys_log_info(
            "[COMBUS_HANDSHAKE] MATCH           wire md5=%s  ver=%u.%u\n",
            wireMd5Hex, (unsigned)wireMajor, (unsigned)wireMinor);
        return;
    }

    // Mismatch — single compact line with local + wire side by side.
    char localMd5Hex[33];
    combus_handshake_formatMd5Hex(combus::wire::kCombusWireMd5, localMd5Hex);

    sys_log_info(
        "[COMBUS_HANDSHAKE] MISMATCH  local md5=%s ver=%u.%u  "
        "wire md5=%s ver=%u.%u\n",
        localMd5Hex,
        (unsigned)combus::wire::kProjectVersionMajor,
        (unsigned)combus::wire::kProjectVersionMinor,
        wireMd5Hex,
        (unsigned)wireMajor,
        (unsigned)wireMinor);

}


// =============================================================================
// 3. RX ENTRY POINT — tryDecode
// =============================================================================

uint8_t combus_handshake_tryDecode(
    uint8_t*       ringBuf,
    uint8_t        ringBufSize,
    uint8_t&       ringHead,
    uint8_t&       ringCount )
{
    if (!ringBuf || ringBufSize == 0u) { return 0u; }
    if (ringCount == 0u)               { return 0u; }

    // Boot banner on first call (cheap flag-guarded).
    combus_handshake_logBootWarningIfNeeded();

    // --- 1. Expected wire length for a handshake frame ---
    const uint8_t expectedLen = CombusFrameHandshakeMinLen;

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
        // Same semantics as combus_rx.cpp: advance by 1 byte past the SOF
        // sentinel so we never loop forever on a corrupted candidate.
        ringHead  = (uint8_t)(ringHead + 1u) % ringBufSize;
        ringCount = (uint8_t)(ringCount - 1u);
        sys_log_info("[COMBUS_HANDSHAKE] CRC mismatch — dropped 1B, re-sync\n");
        return 0u;
    }

    // --- 4. CRC OK — consume the validated frame from the ring buffer ---
    ringHead  = (uint8_t)(ringHead + expectedLen) % ringBufSize;
    ringCount = (uint8_t)(ringCount - expectedLen);

    combus_handshake_internal::markEverReceived();

    // --- 5. Decode payload (MD5 + version) and compare against local ---
    //    Payload starts right after the header, length = kCombusHandshakePayloadLen.
    //    Layout: [0..15] = MD5, [16] = major, [17] = minor.
    const uint8_t payloadOffset = sizeof(CombusFrameSof)
                               + sizeof(CombusFrameHeader);
    const uint8_t* wireMd5   = &linear[payloadOffset];
    const uint8_t  wireMajor = linear[payloadOffset + 16u];
    const uint8_t  wireMinor = linear[payloadOffset + 17u];

    combus_handshake_compareAndLog(wireMd5, wireMajor, wireMinor);

    return expectedLen;
}

// EOF combus_handshake_rx.cpp