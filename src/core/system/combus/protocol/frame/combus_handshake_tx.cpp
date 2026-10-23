/******************************************************************************
 * @file  combus_handshake_tx.cpp
 * @brief TX side — build and transmit ONE handshake frame (seq=0) carrying
 *        the locally-generated MD5 + project version.
 *
 * @details Two entry points: combus_handshake_sendOnce(), a manual one-shot
 *   for debug commands and test fixtures, and combus_handshake_tx_update(),
 *   called by combus_tx_update(): boot burst, one request per second, ACK
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
// TX ENTRY POINT — manual single-shot
// =============================================================================

uint8_t combus_handshake_sendOnce( NodeCom* nodeCom )
{
    // Boot banner on first TX (mirrors RX path).  The TX path does not
    // own a per-link context yet (P3 will introduce one for the burst
    // state) — for now we use a static local context so the banner is
    // emitted exactly once across all TX calls.  This will be replaced
    // by a per-link context in P3 commit 4.
    static CombusHandshakeContext s_txBootCtx = {};
    combus_handshake_logBootWarningIfNeeded(&s_txBootCtx);


    if (!nodeCom || !nodeCom->write) {
        sys_log_info("[COMBUS_HANDSHAKE] TX skipped — null transport\n");
        return 0u;
    }

    // LY4 — use the per-link expected MD5 pointer resolved at init from
    // link->layer.  Falls back to the FULL view MD5 if the pointer is
    // null (legacy caller that bypassed combus_protocol_init).
    const uint8_t* md5ToSend = s_txBootCtx.expectedMd5
        ? s_txBootCtx.expectedMd5
        : combus::wire::kCombusComBusMd5;

    // Frame layout (mirror of RX):
    //   [0]                          SOF
    //   [1..sizeof(Header)]          CombusFrameHeader (nAnalog=0,
    //                                  nDigital=0, seq=0, flags=0)
    //   [..+kCombusHandshakePayloadLen]  MD5 (16) + major + minor
    //   [..+1]                       CRC-8/MAXIM over header + payload
    //
    // nAnalog / nDigital carry no meaning on the handshake path (payload
    // is fixed-length by contract), but the header must stay well-formed
    // because the CRC covers it.  We zero them.
    // RL2: runLevel removed from header — no longer written here.
    uint8_t frame[ sizeof(CombusFrameSof)
                 + sizeof(CombusFrameHeader)
                 + kCombusHandshakePayloadLen
                 + sizeof(uint8_t) ];
    const uint8_t payloadStart = sizeof(CombusFrameSof)
                              + sizeof(CombusFrameHeader);

    // SOF
    frame[0] = CombusFrameSof;

    // Header — nAnalog=0, nDigital=0, seq=0 (RESERVED for handshake),
    // flags=0 (kept deterministic for CRC).
    {
        const CombusFrameHeader hdr = {
            .cfg = { .nAnalog = 0u, .nDigital = 0u },
            .seq = 0u,
            .flags = 0u,
        };
        frame[sizeof(CombusFrameSof) + 0u] = hdr.cfg.nAnalog;
        frame[sizeof(CombusFrameSof) + 1u] = hdr.cfg.nDigital;
        frame[sizeof(CombusFrameSof) + 2u] = hdr.seq;
        frame[sizeof(CombusFrameSof) + 3u] = hdr.flags;
    }

    // Payload — MD5 then version.
    for (uint8_t i = 0u; i < 16u; ++i) {
        frame[payloadStart + i] = md5ToSend[i];
    }
    frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
    frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;


    // CRC-8/MAXIM over header + payload.
    const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;
    frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);

    const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

    char md5Hex[33];
    combus_handshake_formatMd5Hex(md5ToSend, md5Hex);
    sys_log_info(
        "[COMBUS_HANDSHAKE] TX oneshot  bytes=%u  md5=%s  ver=%u.%u\n",
        (unsigned)totalLen, md5Hex,
        (unsigned)combus::wire::kProjectVersionMajor,
        (unsigned)combus::wire::kProjectVersionMinor);


    nodeCom->write(nodeCom->ctx, frame, totalLen);
    return totalLen;
}



// =============================================================================
// TX BURST, REQUESTS AND ACK — P3, chantier 1.2
// =============================================================================

    // Build one handshake frame (local MD5 + version, `flags` as given) and write it.
static uint8_t sendHandshake( const CombusHandshakeContext* ctx, NodeCom* nodeCom, uint8_t flags )
{
        // LY4 — the per-link expected MD5 pointer resolved at init from link->layer;
        // FULL view MD5 if null (legacy caller that bypassed combus_protocol_init).
    const uint8_t* md5ToSend = ctx->expectedMd5
        ? ctx->expectedMd5
        : combus::wire::kCombusComBusMd5;

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
    frame[sizeof(CombusFrameSof) + 3u] = flags;  // kCombusHandshakeFlagAck or 0 (request)

    for (uint8_t i = 0u; i < 16u; ++i) {
        frame[payloadStart + i] = md5ToSend[i];
    }
    frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
    frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;

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
uint8_t combus_handshake_tx_update( CombusHandshakeContext* ctx,
                                    NodeCom*                nodeCom )
{
        // --- 1. Guard checks ---
    if (!ctx || !nodeCom || !nodeCom->write) { return 0u; }

    const uint32_t nowMs   = millis();
    uint8_t        written = 0u;

        // --- 2. A request was received: answer it (never answer an ACK) ---
    if (ctx->ackPending) {
        ctx->ackPending = false;
        written = (uint8_t)(written + sendHandshake(ctx, nodeCom, kCombusHandshakeFlagAck));
    }

        // --- 3. Boot burst: 3 requests 100 ms apart, stopped once the contract is STABLE ---
#if !COMBUS_HANDSHAKE_BURST_DISABLE
    if (ctx->burstActive) {
        if (ctx->contractValidated) {
            ctx->burstActive    = false;
            ctx->burstRemaining = 0u;
        } else if (ctx->lastBurstMs == 0u
                   || (uint32_t)(nowMs - ctx->lastBurstMs) >= kCombusHandshakeBurstPeriodMs) {
            written = (uint8_t)(written + sendHandshake(ctx, nodeCom, 0u));
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
        written = (uint8_t)(written + sendHandshake(ctx, nodeCom, 0u));
        ctx->lastRequestMs = nowMs;
    }

    return written;
}

// EOF combus_handshake_tx.cpp

