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

#include <core/system/combus/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// TX ENTRY POINT — manual single-shot
// =============================================================================

uint8_t combus_handshake_sendOnce( NodeCom* nodeCom )
{
    // Boot banner on first TX (mirrors RX path).
    combus_handshake_logBootWarningIfNeeded();

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
    frame[payloadStart + 16u] = combus::wire::kCombusWireVersionMajor;
    frame[payloadStart + 17u] = combus::wire::kCombusWireVersionMinor;

    // CRC-8/MAXIM over header + payload.
    const uint8_t crcByteIndex = payloadStart + kCombusHandshakePayloadLen;
    frame[crcByteIndex] = combus_frame_crc8(frame, crcByteIndex);

    const uint8_t totalLen = (uint8_t)(crcByteIndex + 1u);

    char md5Hex[33];
    combus_handshake_formatMd5Hex(combus::wire::kCombusWireMd5, md5Hex);
    sys_log_info(
        "[COMBUS_HANDSHAKE] TX oneshot  bytes=%u  md5=%s  ver=%u.%u\n",
        (unsigned)totalLen, md5Hex,
        (unsigned)combus::wire::kCombusWireVersionMajor,
        (unsigned)combus::wire::kCombusWireVersionMinor);

    nodeCom->write(nodeCom->ctx, frame, totalLen);
    return totalLen;
}

// EOF combus_handshake_tx.cpp