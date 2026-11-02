/******************************************************************************
 * @file  combus_handshake_tx.cpp
 * @brief TX side — build and transmit ONE handshake frame (seq=0) carrying
 *        the locally-generated MD5 + project version.
 *
 * @details Two entry points: combus_handshake_sendOnce(), a manual one-shot
 *   for debug commands and test fixtures, and combus_handshake_tx_update(),
 *   called by combus_tx_update(): boot burst, one request per period, ACK
 *   of the received requests (chantier 1.2).
 *
 *   Format-md5 helper (`combus_handshake_formatMd5Hex`) is owned by
 *   combus_handshake.cpp.  Boot banner is also owned by the umbrella.
 *****************************************************************************/

#include "combus_handshake_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>



// =============================================================================
// TX BURST, REQUESTS AND ACK — P3, chantier 1.2
// =============================================================================

    // Build one handshake frame (local MD5 + version, `flags` as given) and write it.
    // A NEXT handshake carries the next set MD5 (chantier 2.2b).
static uint8_t sendHandshake( const CombusHandshakeContext* ctx, NodeCom* nodeCom, uint8_t flags )
{
    const uint8_t* stableMd5 = ctx->expected.stable;
    const uint8_t* nextMd5   = ctx->expected.next;
        // Link rework 3.4a — through a coupler, this end announces its own role.
    if (ctx->announced.stable) { stableMd5 = ctx->announced.stable; }
    if (ctx->announced.next)   { nextMd5   = ctx->announced.next; }
    const uint8_t* md5ToSend = ((flags & kCombusHandshakeFlagNext) && nextMd5)
        ? nextMd5
        : stableMd5;

    uint8_t frame[ sizeof(CombusFrameSof)
                 + sizeof(CombusFrameHeader)
                 + kCombusHandshakePayloadLen
                 + sizeof(uint8_t) ];
    const uint8_t payloadStart = sizeof(CombusFrameSof)
                              + sizeof(CombusFrameHeader);

    frame[0] = CombusFrameSof;
    frame[sizeof(CombusFrameSof) + 0u] = 0u;     // nAnalog
    frame[sizeof(CombusFrameSof) + 1u] = 0u;     // nDigital
    frame[sizeof(CombusFrameSof) + 2u] = 0u;     // seq = 0 (RESERVED handshake)
    frame[sizeof(CombusFrameSof) + 3u] = flags;  // ACK and / or NEXT bits, 0 = legacy request

    for (uint8_t i = 0u; i < 16u; ++i) {
        frame[payloadStart + i] = md5ToSend[i];
    }
    frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
    frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;
    frame[payloadStart + 18u] = combus::wire::kProjectVersionDev;

    const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;
    frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);
    const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

    nodeCom->write(nodeCom->ctx, frame, totalLen);
    return totalLen;
}


/**
 * @brief Drive the handshake emission of the given ComBus link.
 *
 * @details See combus_handshake_tx.h for the full contract.  Does NOT touch
 *   the control-frame timer: the frames below are extra `write()` calls.
 *
 *   Per-link state — see CombusHandshakeContext.
 */
    // Kind of the next request (chantier 2.2b): while UNKNOWN, NEXT as long as
    // NEXT tries are left; once STABLE, the kind of the mode.
static uint8_t requestFlags( CombusHandshakeContext* ctx )
{
    if (ctx->contractValidated) { return ctx->nextMode ? kCombusHandshakeFlagNext : 0u; }
    if (ctx->nextTriesLeft > 0u) {
        ctx->nextTriesLeft--;
        return kCombusHandshakeFlagNext;
    }
    return 0u;
}


uint8_t combus_handshake_tx_update( CombusHandshakeContext* ctx,
                                    NodeCom*                nodeCom )
{
        // --- 1. Guard checks ---
    if (!ctx || !nodeCom || !nodeCom->write || !ctx->expected.stable) { return 0u; }

    const uint32_t nowMs   = millis();
    uint8_t        written = 0u;

        // --- 2. A request was received: answer it with an ACK of its kind (never answer an ACK) ---
    if (ctx->ackPending) {
        ctx->ackPending = false;
        written = (uint8_t)(written + sendHandshake(ctx, nodeCom, kCombusHandshakeFlagAck));
    }
    if (ctx->ackPendingNext) {
        ctx->ackPendingNext = false;
        written = (uint8_t)(written + sendHandshake(ctx, nodeCom,
                                                    kCombusHandshakeFlagAck | kCombusHandshakeFlagNext));
    }

        // --- 3. Boot burst: 3 requests 100 ms apart, stopped once the contract is STABLE ---
#if !COMBUS_HANDSHAKE_BURST_DISABLE
    if (ctx->burstActive && ctx->contractValidated) {
        ctx->burstActive    = false;
        ctx->burstRemaining = 0u;
    }
    if (ctx->burstActive) {
        if (ctx->lastBurstMs == 0u
            || (uint32_t)(nowMs - ctx->lastBurstMs) >= kCombusHandshakeBurstPeriodMs) {
            written = (uint8_t)(written + sendHandshake(ctx, nodeCom, requestFlags(ctx)));
            ctx->lastBurstMs   = nowMs;
            ctx->lastRequestMs = nowMs;
            if (ctx->burstRemaining > 0u) { ctx->burstRemaining--; }
            if (ctx->burstRemaining == 0u) { ctx->burstActive = false; }   // burst complete
        }
        return written;
    }
#endif

        // --- 4. Then one request per period, whatever the state of the contract ---
    if ((uint32_t)(nowMs - ctx->lastRequestMs) >= kCombusHandshakePeriodMs) {
        written = (uint8_t)(written + sendHandshake(ctx, nodeCom, requestFlags(ctx)));
        ctx->lastRequestMs = nowMs;
    }

    return written;
}

// EOF combus_handshake_tx.cpp

