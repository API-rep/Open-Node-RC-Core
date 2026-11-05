/******************************************************************************
 * @file  combus_coupling.cpp
 * @brief ComBus coupling frame: encoding, decoding, routing.
 *
 * @details The wire layout is in combus_coupling.h.
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

	// The last frame received on one link, for the tests and the diagnostics.
struct CouplingRxRecord {
	uint32_t      count = 0u;
	bool          valid = false;
	CouplingFrame last  = {};
};

CouplingRxRecord s_rx[kCombusCouplingMaxLinks];
CouplingRxHandler s_handler = nullptr;

constexpr uint8_t kPayloadStart = CombusFrameHeaderLen;


	/// Byte at a logical position of the ring; the sum is taken on an int, before the modulo.
uint8_t ringByteAt( const uint8_t* ringBuf, uint8_t ringBufSize, uint8_t ringHead, uint8_t i )
{
	return ringBuf[(uint8_t)((ringHead + i) % ringBufSize)];
}


	/// Request or answer: any other value is refused.
bool knownMsg( uint8_t msg )
{
	return msg == static_cast<uint8_t>(CouplingMsg::REQUEST)
		|| msg == static_cast<uint8_t>(CouplingMsg::ANSWER);
}

} // namespace



// =============================================================================
// 2. ENCODE AND DECODE
// =============================================================================

	/// 65535 means "at least 65535 ms": two ages that old always agree.
uint16_t combus_coupling_hitch_age( uint32_t ms )
{
	return (ms > 0xFFFFu) ? (uint16_t)0xFFFFu : (uint16_t)ms;
}


/**
 * @brief Coupling frame encoder. Header, payload, CRC.
 *
 * @details In order:
 *   1. SOF and header: no channel, `seq` 0 as the handshake, and the coupling flag.
 *   2. Payload, integers little-endian.
 *   3. CRC over SOF, header and payload, as every ComBus frame.
 */

uint8_t combus_coupling_encode( uint8_t* buf, uint8_t bufLen, const CouplingFrame& frame )
{
	if (!buf || bufLen < kCombusCouplingFrameLen) { return 0u; }

		// 1. SOF and header
	buf[0] = CombusFrameSof;
	buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog)]  = 0u;
	buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital)] = 0u;
	buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, seq)]   = 0u;
	buf[sizeof(CombusFrameSof) + offsetof(CombusFrameHeader, flags)] = kCombusFrameFlagCoupling;

		// 2. Payload
	uint8_t* p = &buf[kPayloadStart];
	p[0] = static_cast<uint8_t>(frame.msg);
	p[1] = frame.round;
	p[2] = static_cast<uint8_t>(frame.addr.type);
	memcpy(&p[3], frame.addr.bytes, 6u);
	p[9]  = (uint8_t)(frame.hitchAgeMs & 0xFFu);
	p[10] = (uint8_t)((frame.hitchAgeMs >> 8u) & 0xFFu);
	p[11] = (uint8_t)(frame.features & 0xFFu);
	p[12] = (uint8_t)((frame.features >> 8u) & 0xFFu);

		// 3. CRC
	const uint8_t crcIndex = kPayloadStart + kCombusCouplingPayloadLen;
	buf[crcIndex] = combus_frame_crc8(buf, crcIndex);

	return kCombusCouplingFrameLen;
}


/**
 * @brief Coupling frame decoder. Checks the frame, then reads its payload.
 *
 * @details In order:
 *   1. Length and SOF.
 *   2. CRC, before reading any field.
 *   3. Header of a coupling frame: no channel, `seq` 0, the coupling flag set.
 *   4. A known message, then the payload.
 */

bool combus_coupling_decode( const uint8_t* buf, uint8_t len, CouplingFrame& out )
{
		// 1. Length and SOF
	if (!buf || len != kCombusCouplingFrameLen || buf[0] != CombusFrameSof) { return false; }

		// 2. CRC
	const uint8_t crcIndex = kPayloadStart + kCombusCouplingPayloadLen;

	if (combus_frame_crc8(buf, crcIndex) != buf[crcIndex]) { return false; }

		// 3. Header
	const uint8_t* h = &buf[sizeof(CombusFrameSof)];

	if (h[offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog)]  != 0u
	 || h[offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital)] != 0u
	 || h[offsetof(CombusFrameHeader, seq)] != 0u
	 || (h[offsetof(CombusFrameHeader, flags)] & kCombusFrameFlagCoupling) == 0u) {
		return false;
	}

		// 4. Message and payload
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
// 3. RECEPTION
// =============================================================================

/**
 * @brief Coupling frame reception. From the ring of combus_rx to the pairing.
 *
 * @details In order:
 *   1. Wait for the whole frame, then copy it out of the ring.
 *   2. Refused: drop the SOF only, so that the stream re-syncs, as the other decoders.
 *   3. Accepted: consume it, record it when its link is recorded, hand it to the handler.
 */

uint8_t combus_coupling_tryDecode( uint8_t  linkIdx,
                                   uint8_t* ringBuf,
                                   uint8_t  ringBufSize,
                                   uint8_t& ringHead,
                                   uint8_t& ringCount )
{
	if (!ringBuf || ringBufSize == 0u) { return 0u; }

		// 1. Whole frame
	if (ringCount < kCombusCouplingFrameLen) { return 0u; }

	uint8_t linear[kCombusCouplingFrameLen];

		// Copy each byte out of the ring
	for (uint8_t i = 0u; i < kCombusCouplingFrameLen; ++i) {
		linear[i] = ringByteAt(ringBuf, ringBufSize, ringHead, i);
	}

		// 2. Refused
	CouplingFrame frame;

	if (!combus_coupling_decode(linear, kCombusCouplingFrameLen, frame)) {
		ringHead  = (uint8_t)((ringHead + 1u) % ringBufSize);
		ringCount = (uint8_t)(ringCount - 1u);
		sys_log_info("[COMBUS_COUPLING] invalid coupling frame — dropped 1B, re-sync\n");

		return 0u;
	}

		// 3. Accepted
	ringHead  = (uint8_t)((ringHead + kCombusCouplingFrameLen) % ringBufSize);
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
// 4. IDENTITY, RECORDS AND HANDLER
// =============================================================================

	/// The station MAC: the same on a wire and over the air.
NodeAddr combus_coupling_local_addr( void )
{
	NodeAddr addr;

	if (esp_read_mac(addr.bytes, ESP_MAC_WIFI_STA) == ESP_OK) { addr.type = LinkAddrType::MAC48; }

	return addr;
}


	/// 0 beyond the recorded links.
uint32_t combus_coupling_rx_count( uint8_t linkIdx )
{
	return (linkIdx < kCombusCouplingMaxLinks) ? s_rx[linkIdx].count : 0u;
}


	/// false beyond the recorded links, or with nothing received.
bool combus_coupling_last_rx( uint8_t linkIdx, CouplingFrame& out )
{
	if (linkIdx >= kCombusCouplingMaxLinks || !s_rx[linkIdx].valid) { return false; }

	out = s_rx[linkIdx].last;

	return true;
}


	/// Every record back to empty; the handler stays registered.
void combus_coupling_reset( void )
{
	for (auto& r : s_rx) { r = CouplingRxRecord{}; }
}


	/// One handler for every link: it receives the index of the link.
void combus_coupling_set_rx_handler( CouplingRxHandler handler )
{
	s_handler = handler;
}

// EOF combus_coupling.cpp
