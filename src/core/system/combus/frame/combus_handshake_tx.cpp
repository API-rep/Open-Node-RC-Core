/******************************************************************************
 * @file  combus_handshake_tx.cpp
 * @brief TX side — build and transmit ONE handshake frame (seq=0) carrying
 *        the locally-generated MD5 + project version.
 *
 * @details Manual one-shot — no automatic burst.  Intended for debug
 *   commands, test fixtures, or future user-triggered handshake.  Log
 *   lines are kept on a single line per event for grep-friendly
 *   boot-sequence reading.
 *
 *   Format-md5 helper (`combus_handshake_formatMd5Hex`) is owned by
 *   combus_handshake.cpp.  Boot banner is also owned by the umbrella.
 *
 *   Scope EXPLICITLY out of this revision:
 *     - automatic burst at boot;
 *     - periodic re-send / keepalive.
 *****************************************************************************/

#include "combus_handshake_tx.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/frame/combus_frame.h>
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

    // Frame layout (mirror of RX):
    //   [0]                          SOF
    //   [1..sizeof(Header)]          CombusFrameHeader (nAnalog=0,
    //                                  nDigital=0, seq=0, runLevel=0)
    //   [..+kCombusHandshakePayloadLen]  MD5 (16) + major + minor
    //   [..+1]                       CRC-8/MAXIM over header + payload
    //
    // nAnalog / nDigital carry no meaning on the handshake path (payload
    // is fixed-length by contract), but the header must stay well-formed
    // because the CRC covers it.  We zero them.
    uint8_t frame[ sizeof(CombusFrameSof)
                 + sizeof(CombusFrameHeader)
                 + kCombusHandshakePayloadLen
                 + sizeof(uint8_t) ];
    const uint8_t payloadStart = sizeof(CombusFrameSof)
                              + sizeof(CombusFrameHeader);

    // SOF
    frame[0] = CombusFrameSof;

    // Header — nAnalog=0, nDigital=0, seq=0 (RESERVED for handshake),
    // runLevel=0 (kept deterministic for CRC).
    {
        const CombusFrameHeader hdr = {
            .cfg = { .nAnalog = 0u, .nDigital = 0u },
            .seq = 0u,
            .runLevel = 0u,
        };
        frame[sizeof(CombusFrameSof) + 0u] = hdr.cfg.nAnalog;
        frame[sizeof(CombusFrameSof) + 1u] = hdr.cfg.nDigital;
        frame[sizeof(CombusFrameSof) + 2u] = hdr.seq;
        frame[sizeof(CombusFrameSof) + 3u] = hdr.runLevel;
    }

    // Payload — MD5 then version.
    for (uint8_t i = 0u; i < 16u; ++i) {
        frame[payloadStart + i] = combus::wire::kCombusWireMd5[i];
    }
    frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
    frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;


    // CRC-8/MAXIM over header + payload.
    const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;
    frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);

    const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

    char md5Hex[33];
    combus_handshake_formatMd5Hex(combus::wire::kCombusWireMd5, md5Hex);
    sys_log_info(
        "[COMBUS_HANDSHAKE] TX oneshot  bytes=%u  md5=%s  ver=%u.%u\n",
        (unsigned)totalLen, md5Hex,
        (unsigned)combus::wire::kProjectVersionMajor,
        (unsigned)combus::wire::kProjectVersionMinor);


    nodeCom->write(nodeCom->ctx, frame, totalLen);
    return totalLen;
}



// =============================================================================
// TX BURST — P3
// =============================================================================

/**
 * @brief Drive the boot-time handshake burst on the given ComBus link.
 *
 * @details See combus_handshake_tx.h for the full contract.  This
 *   implementation is intentionally minimal — it does NOT touch the
 *   control-frame timer, does NOT introduce any priority / contention
 *   logic, and does NOT modify the existing `combus_tx_update()` flow
 *   beyond a single extra `write()` call when the burst fires.
 *
 *   Per-link state — see CombusHandshakeContext.  The burst state is
 *   owned by the caller (typically embedded in CombusTxState).
 */
uint8_t combus_handshake_tx_update( CombusHandshakeContext* ctx,
                                    NodeCom*                nodeCom )
{
    // --- 1. Guard checks ---
    if (!ctx || !nodeCom || !nodeCom->write) { return 0u; }

#if COMBUS_HANDSHAKE_BURST_DISABLE
    // Compile-time bypass — burst disabled.  The contract must be
    // validated by the peer sending a handshake frame first.
    return 0u;
#else
    // --- 2. Burst must be active ---
    if (!ctx->burstActive) { return 0u; }

    // --- 3. Early termination — contract already validated on this link.
    //    P3 constraint #3: a full-duplex link stops emitting as soon as
    //    the peer has confirmed the contract.  We assume full-duplex
    //    (mechanism B — see WIP combus_v2 §5.3) because every caller
    //    today shares the same NodeCom* for TX and RX via
    //    combus_protocol_init.  When N2 (asymmetric transports) lands,
    //    this becomes a per-link flag passed to combus_tx_init().
    if (ctx->contractValidated) {
        ctx->burstActive    = false;
        ctx->burstRemaining = 0u;
        return 0u;
    }

    // --- 4. Interval gate — at least kCombusHandshakeBurstPeriodMs since
    //    the last emission.  lastBurstMs == 0 means "never emitted yet"
    //    (set by startBurst) → first call always fires.
    const uint32_t nowMs = millis();
    if (ctx->lastBurstMs != 0u
        && (uint32_t)(nowMs - ctx->lastBurstMs) < kCombusHandshakeBurstPeriodMs) {
        return 0u;
    }

    // --- 5. Emit one handshake frame via the existing sendOnce path.
    //    We do NOT call sendOnce() directly because it would re-emit the
    //    boot banner on every burst frame.  Instead we inline the
    //    minimal frame build + write here.
    uint8_t frame[ sizeof(CombusFrameSof)
                 + sizeof(CombusFrameHeader)
                 + kCombusHandshakePayloadLen
                 + sizeof(uint8_t) ];
    const uint8_t payloadStart = sizeof(CombusFrameSof)
                              + sizeof(CombusFrameHeader);

    frame[0] = CombusFrameSof;
    frame[sizeof(CombusFrameSof) + 0u] = 0u;  // nAnalog
    frame[sizeof(CombusFrameSof) + 1u] = 0u;  // nDigital
    frame[sizeof(CombusFrameSof) + 2u] = 0u;  // seq = 0 (RESERVED handshake)
    frame[sizeof(CombusFrameSof) + 3u] = 0u;  // runLevel

    for (uint8_t i = 0u; i < 16u; ++i) {
        frame[payloadStart + i] = combus::wire::kCombusWireMd5[i];
    }
    frame[payloadStart + 16u] = combus::wire::kProjectVersionMajor;
    frame[payloadStart + 17u] = combus::wire::kProjectVersionMinor;

    const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;
    frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);
    const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

    nodeCom->write(nodeCom->ctx, frame, totalLen);

    // --- 6. Update burst state ---
    ctx->lastBurstMs = nowMs;
    if (ctx->burstRemaining > 0u) { ctx->burstRemaining--; }
    if (ctx->burstRemaining == 0u) {
        ctx->burstActive = false;  // burst complete
    }

    return totalLen;
#endif
}

// EOF combus_handshake_tx.cpp

