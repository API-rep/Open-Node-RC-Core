/******************************************************************************
 * @file  combus_handshake_rx.cpp
 * @brief RX side — decode a handshake frame from the ring buffer, compare
 *        the MD5 + major version against the locally-generated copy, update
 *        the contract of the link (chantier 1.2).
 *
 * @details Format-md5 helper (`combus_handshake_formatMd5Hex`) is owned by
 *   combus_handshake.cpp.  Boot banner is also owned by the umbrella and
 *   invoked on the first decode attempt.
 *
 *   Scope EXPLICITLY out of this revision (to be added later):
 *     - friend-cache update ("amies" / sender address);
 *     - high-priority QoS routing.
 *****************************************************************************/

#include "combus_handshake_rx.h"

#include <string.h>
#include <stddef.h>
#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
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

/**
 * @brief Compare an on-wire MD5 + major version against the locally-generated
 *        copy.  See combus_handshake_rx.h.
 */
bool combus_handshake_compare(
    const uint8_t* localMd5,
    const uint8_t* wireMd5,
    uint8_t        wireMajor )
{
        // Bring-up bypass: every handshake is accepted (banner logged at boot).
    if (COMBUS_MD5_CHECK_DISABLE) { return true; }

        // LY4 — a null local MD5 means the link was not initialised through
        // combus_protocol_init(): config error, never substituted silently.
    if (!localMd5) {
        sys_log_err(
            "[COMBUS_HANDSHAKE] FATAL: CombusHandshakeContext::expectedMd5 is null "
            "— link was not properly initialized via combus_protocol_init() "
            "— system halted\n");
        while (1) { /* halt */ }
    }

    return (memcmp(wireMd5, localMd5, 16) == 0)
        && (wireMajor == combus::wire::kProjectVersionMajor);
}


    // Log a refused handshake once, until the contract is STABLE again.
static void logRefusal( CombusHandshakeContext* ctx, const uint8_t* wireMd5,
                        uint8_t wireMajor, uint8_t wireMinor )
{
    if (ctx->refusalLogged) { return; }
    ctx->refusalLogged = true;

    char localMd5Hex[33];
    char wireMd5Hex[33];
    combus_handshake_formatMd5Hex(ctx->expectedMd5, localMd5Hex);
    combus_handshake_formatMd5Hex(wireMd5, wireMd5Hex);

    sys_log_warn(
        "[COMBUS_HANDSHAKE] link '%s' REFUSED  local md5=%s ver=%u.%u  wire md5=%s ver=%u.%u\n",
        ctx->name ? ctx->name : "?",
        localMd5Hex,
        (unsigned)combus::wire::kProjectVersionMajor,
        (unsigned)combus::wire::kProjectVersionMinor,
        wireMd5Hex, (unsigned)wireMajor, (unsigned)wireMinor);
}



// =============================================================================
// 3. RX ENTRY POINT — tryDecode
// =============================================================================

uint8_t combus_handshake_tryDecode(
    CombusHandshakeContext* ctx,
    uint8_t*                ringBuf,
    uint8_t                 ringBufSize,
    uint8_t&                ringHead,
    uint8_t&                ringCount )
{
    if (!ctx || !ringBuf || ringBufSize == 0u) { return 0u; }
    if (ringCount == 0u)                       { return 0u; }

    // Boot banner on first call (cheap flag-guarded, per-link).
    combus_handshake_logBootWarningIfNeeded(ctx);


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

    combus_handshake_internal::markEverReceived(ctx);

    // --- 5. Request or ACK, legacy or NEXT; MD5 + major compared on EVERY handshake.
    //    Layout: [0..15] = MD5 (next set for a NEXT handshake), [16] = major,
    //    [17] = minor, [18] = dev.
    const uint8_t  flags         = linear[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, flags)];
    const bool     isAck         = (flags & kCombusHandshakeFlagAck)  != 0u;
    const bool     isNext        = (flags & kCombusHandshakeFlagNext) != 0u;
    const uint8_t  payloadOffset = sizeof(CombusFrameSof)
                                 + sizeof(CombusFrameHeader);
    const uint8_t* wireMd5   = &linear[payloadOffset];
    const uint8_t  wireMajor = linear[payloadOffset + 16u];
    const uint8_t  wireMinor = linear[payloadOffset + 17u];
    const uint8_t  wireDev   = linear[payloadOffset + 18u];

    // --- 6. A bidirectional end answers every request with an ACK of its kind,
    //    matching or not (never an ACK).
    if (!isAck && ctx->bidirectional) {
        if (isNext) { ctx->ackPendingNext = true; }
        else        { ctx->ackPending     = true; }
    }

    // --- 7. NEXT handshake (chantier 2.2b): it only decides the mode, never the
    //    contract.  Without candidates this end stays legacy.
    if (isNext) {
        const bool nextMatch = ctx->hasCandidates
                            && combus_handshake_compare(ctx->expectedMd5Next, wireMd5, wireMajor);
        if (!nextMatch) {
            if (ctx->nextMode) {
                ctx->nextMode = false;
                sys_log_info("[COMBUS_HANDSHAKE] link '%s' mode stable (the peer announces another next set)\n",
                             ctx->name ? ctx->name : "?");
            }
            if (isAck && !ctx->contractValidated) { ctx->nextTriesLeft = 0u; }   // legacy at once
            return expectedLen;
        }

        if (isAck) { ctx->lastAckMs = millis(); }

        if (ctx->contractValidated) {
                // The mode follows what the peer announces: the two ends meet in one period
            if (!ctx->nextMode) {
                ctx->nextMode = true;
                sys_log_info("[COMBUS_HANDSHAKE] link '%s' mode next (the peer announces the same next set)\n",
                             ctx->name ? ctx->name : "?");
            }
            return expectedLen;
        }

            // UNKNOWN: a bidirectional end needs the ACK of its own request
        if (ctx->bidirectional && !isAck) { return expectedLen; }
        ctx->nextMode = true;
    } else {

        // --- 8. Legacy handshake: refused, the contract is (back to) UNKNOWN, logged once.
        if (!combus_handshake_compare(ctx->expectedMd5, wireMd5, wireMajor)) {
            logRefusal(ctx, wireMd5, wireMajor, wireMinor);
            combus_handshake_internal::dropContract(ctx, "handshake refused");
            return expectedLen;
        }

        // Accepted.  A bidirectional end needs the ACK of its own request (proof
        // that the peer receives it); a receive-only end takes the request.
        if (ctx->bidirectional && !isAck) { return expectedLen; }

        if (isAck) { ctx->lastAckMs = millis(); }
        if (ctx->contractValidated) { return expectedLen; }
        ctx->nextMode = false;
    }

    // --- 9. The contract becomes STABLE, in the mode chosen above ---
    combus_handshake_internal::markContractValidated(ctx);
    combus_handshake_internal::stopBurst(ctx);
    ctx->lastAckMs       = millis();     // the ACK window starts now
    ctx->dataSinceStable = false;
    ctx->refusalLogged   = false;

    sys_log_info("[COMBUS_HANDSHAKE] link '%s' contract STABLE  mode %s  wire ver=%u.%u dev=%u%s\n",
                 ctx->name ? ctx->name : "?",
                 ctx->nextMode ? "next" : (ctx->hasCandidates ? "stable" : "legacy (no candidate)"),
                 (unsigned)wireMajor, (unsigned)wireMinor, (unsigned)wireDev,
                 (wireMinor != combus::wire::kProjectVersionMinor || wireDev != combus::wire::kProjectVersionDev)
                     ? "  (minor or dev version differs)" : "");

    return expectedLen;
}



// EOF combus_handshake_rx.cpp