/******************************************************************************
 * @file  combus_coupling.cpp
 * @brief ComBus coupling frame: encoding, decoding, routing (link rework 3.4b).
 *
 * @details See combus_coupling.h for the wire layout.  Step 3.4b records the
 *   last received frame of each link; the pairing is step 3.4c.
 *****************************************************************************/

#include "combus_coupling.h"

#include <string.h>
#include <stddef.h>
#include <esp_mac.h>   // esp_read_mac()

#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. INTERNAL STATE AND HELPERS
// =============================================================================

namespace {

struct CouplingRxRecord {
    uint32_t      count = 0u;
    bool          valid = false;
    CouplingFrame last  = {};
};

CouplingRxRecord s_rx[kCombusCouplingMaxLinks];
CouplingRxHandler s_handler = nullptr;

constexpr uint8_t kPayloadStart = CombusFrameHeaderLen;

uint8_t ringByteAt( const uint8_t* ringBuf, uint8_t ringBufSize, uint8_t ringHead, uint8_t i )
{
    return ringBuf[(uint8_t)((ringHead + i) % ringBufSize)];
}

bool knownMsg( uint8_t msg )
{
    return msg == static_cast<uint8_t>(CouplingMsg::REQUEST)
        || msg == static_cast<uint8_t>(CouplingMsg::ANSWER);
}

} // namespace


// =============================================================================
// 2. ENCODE / DECODE
// =============================================================================

uint16_t combus_coupling_hitch_age( uint32_t ms )
{
    return (ms > 0xFFFFu) ? (uint16_t)0xFFFFu : (uint16_t)ms;
}


uint8_t combus_coupling_encode( uint8_t* buf, uint8_t bufLen, const CouplingFrame& frame )
{
    if (!buf || bufLen < kCombusCouplingFrameLen) { return 0u; }

        // --- 1. SOF and header: no channel, seq 0 (as the handshake), the coupling flag ---
    buf[0] = CombusFrameSof;
    buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog)]  = 0u;
    buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital)] = 0u;
    buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, seq)]   = 0u;
    buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, flags)] = kCombusFrameFlagCoupling;

        // --- 2. Payload ---
    uint8_t* p = &buf[kPayloadStart];
    p[0] = static_cast<uint8_t>(frame.msg);
    p[1] = frame.round;
    p[2] = static_cast<uint8_t>(frame.addr.type);
    memcpy(&p[3], frame.addr.bytes, 6u);
    p[9]  = (uint8_t)(frame.hitchAgeMs & 0xFFu);
    p[10] = (uint8_t)((frame.hitchAgeMs >> 8u) & 0xFFu);
    p[11] = (uint8_t)(frame.features & 0xFFu);
    p[12] = (uint8_t)((frame.features >> 8u) & 0xFFu);

        // --- 3. CRC over SOF + header + payload, as every ComBus frame ---
    const uint8_t crcIndex = kPayloadStart + kCombusCouplingPayloadLen;
    buf[crcIndex] = combus_frame_crc8(buf, crcIndex);
    return kCombusCouplingFrameLen;
}


bool combus_coupling_decode( const uint8_t* buf, uint8_t len, CouplingFrame& out )
{
    if (!buf || len != kCombusCouplingFrameLen || buf[0] != CombusFrameSof) { return false; }

    const uint8_t crcIndex = kPayloadStart + kCombusCouplingPayloadLen;
    if (combus_frame_crc8(buf, crcIndex) != buf[crcIndex]) { return false; }

    const uint8_t* h = &buf[sizeof(CombusFrameSof)];
    if (h[offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog)]  != 0u
     || h[offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital)] != 0u
     || h[offsetof(CombusFrameHeader, seq)] != 0u
     || (h[offsetof(CombusFrameHeader, flags)] & kCombusFrameFlagCoupling) == 0u) {
        return false;
    }

    const uint8_t* p = &buf[kPayloadStart];
    if (!knownMsg(p[0])) { return false; }

    out.msg        = static_cast<CouplingMsg>(p[0]);
    out.round      = p[1];
    out.addr.type  = static_cast<LinkAddrType>(p[2]);
    memcpy(out.addr.bytes, &p[3], 6u);
    out.hitchAgeMs = (uint16_t)(p[9]  | (uint16_t)(p[10] << 8u));
    out.features   = (uint16_t)(p[11] | (uint16_t)(p[12] << 8u));
    return true;
}


// =============================================================================
// 3. RX ENTRY POINT — called by combus_rx for a seq == 0 frame with the coupling flag
// =============================================================================

uint8_t combus_coupling_tryDecode( uint8_t  linkIdx,
                                   uint8_t* ringBuf,
                                   uint8_t  ringBufSize,
                                   uint8_t& ringHead,
                                   uint8_t& ringCount )
{
    if (!ringBuf || ringBufSize == 0u)        { return 0u; }
    if (ringCount < kCombusCouplingFrameLen)  { return 0u; }   // incomplete: retry on the next poll

        // --- 1. Linearise the candidate frame ---
    uint8_t linear[kCombusCouplingFrameLen];
    for (uint8_t i = 0u; i < kCombusCouplingFrameLen; ++i) {
        linear[i] = ringByteAt(ringBuf, ringBufSize, ringHead, i);
    }

        // --- 2. Refused: drop the SOF only and re-sync (as the other decoders) ---
    CouplingFrame frame;
    if (!combus_coupling_decode(linear, kCombusCouplingFrameLen, frame)) {
        ringHead  = (uint8_t)(ringHead + 1u) % ringBufSize;
        ringCount = (uint8_t)(ringCount - 1u);
        sys_log_info("[COMBUS_COUPLING] invalid coupling frame — dropped 1B, re-sync\n");
        return 0u;
    }

        // --- 3. Accepted: consume, record, hand to the pairing (3.4c) ---
    ringHead  = (uint8_t)(ringHead + kCombusCouplingFrameLen) % ringBufSize;
    ringCount = (uint8_t)(ringCount - kCombusCouplingFrameLen);
    if (linkIdx < kCombusCouplingMaxLinks) {
        s_rx[linkIdx].count++;
        s_rx[linkIdx].valid = true;
        s_rx[linkIdx].last  = frame;
    }
    if (s_handler) { s_handler(linkIdx, frame); }
    return kCombusCouplingFrameLen;
}


// =============================================================================
// 4. IDENTITY AND RECORDS
// =============================================================================

NodeAddr combus_coupling_local_addr( void )
{
    NodeAddr addr;
    if (esp_read_mac(addr.bytes, ESP_MAC_WIFI_STA) == ESP_OK) { addr.type = LinkAddrType::MAC48; }
    return addr;
}


uint32_t combus_coupling_rx_count( uint8_t linkIdx )
{
    return (linkIdx < kCombusCouplingMaxLinks) ? s_rx[linkIdx].count : 0u;
}


bool combus_coupling_last_rx( uint8_t linkIdx, CouplingFrame& out )
{
    if (linkIdx >= kCombusCouplingMaxLinks || !s_rx[linkIdx].valid) { return false; }
    out = s_rx[linkIdx].last;
    return true;
}


void combus_coupling_reset( void )
{
    for (auto& r : s_rx) { r = CouplingRxRecord{}; }
}


void combus_coupling_set_rx_handler( CouplingRxHandler handler )
{
    s_handler = handler;
}

// EOF combus_coupling.cpp
