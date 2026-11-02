/******************************************************************************
 * @file  combus_frame.cpp
 * @brief ComBus control frame codec: encode, decode, CRC-8 and bus application.
 *
 * @details The codec is pure: it knows neither the state of a link nor its loss,
 *   which the link layer watches (com_rx_update() publishes NODE_LINK_FAIL and
 *   BOARD_LINK_FAIL).  It knows no channel in particular either: the run level and
 *   the failsafe travel as ordinary channels.
 *****************************************************************************/

#include "combus_frame.h"

#include <string.h>
#include <Arduino.h>
#include <core/system/combus/combus_access.h>
#include <core/config/machines/combus_types.h>  // AnalogComBusID, DigitalComBusID



// =============================================================================
// 1. CRC
// =============================================================================

    // Polynomial 0x31 (reflected 0x8C), init 0x00.  Byte by byte: no lookup table
    // is needed at the frame sizes used here.
uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len) {
	uint8_t crc = 0x00;
	for (uint8_t i = 0; i < len; ++i) {
		uint8_t byte = data[i];
		for (uint8_t b = 0; b < 8u; ++b) {
			uint8_t mix = (crc ^ byte) & 0x01u;
			crc >>= 1u;
			if (mix) {
				crc ^= 0x8Cu;
			}
			byte >>= 1u;
		}
	}
	return crc;
}



// =============================================================================
// 2. ENCODE
// =============================================================================

uint8_t combus_frame_encode( const ComBusFrameCfg& cfg,
                             uint8_t*              outputBuffer,
                             const ComBus*         combus,
                             uint8_t               seq,
                             uint8_t               analogWireEnd,
                             uint8_t               digitalWireEnd,
                             const bool*           digitalValues,
                             const uint8_t*        analogMap,
                             const uint8_t*        digitalMap ) {

		// 1. Channel counts of the frame, clamped to the layout
	const uint8_t nAnalog  = (analogWireEnd  < cfg.nAnalog)  ? analogWireEnd  : cfg.nAnalog;
	const uint8_t nDigital = (digitalWireEnd < cfg.nDigital) ? digitalWireEnd : cfg.nDigital;

		// 2. Guard conditions
	if (!outputBuffer || !combus) {
		return 0;
	}
			// seq 0 is reserved: this frame would be taken for a handshake on the other end
	if (seq == 0u) {
		return 0;
	}
			// The frame length must fit a uint8_t
	uint8_t nDigBytes = (nDigital + 7u) / 8u;   // ceil(nDigital / 8)

	if ((CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)nAnalog * 2u + 1u) > 255u) {
		return 0;
	}

		// 3. Flags: no bit defined on a control frame
	uint8_t flags = 0;   // transport-level status bits

		// 4. Fixed header
	uint8_t pos = 0;   // write position in outputBuffer

	outputBuffer[pos++] = CombusFrameSof;
	outputBuffer[pos++] = nAnalog;
	outputBuffer[pos++] = nDigital;
	outputBuffer[pos++] = seq;
	outputBuffer[pos++] = flags;

		// 5. Digital values, packed as bits (LSB first)
	for (uint8_t b = 0; b < nDigBytes; ++b) {
		uint8_t packed = 0;

		for (uint8_t bit = 0; bit < 8u; ++bit) {
			uint8_t ch = (uint8_t)(b * 8u + bit);

			if (ch >= nDigital) { continue; }

				// The values prepared for this link (both_or without echo) when given,
				// the bus values otherwise (through the table of a link between nodes).
			const uint8_t id = digitalMap ? digitalMap[ch] : ch;
			const bool v = digitalValues ? digitalValues[ch]
			                             : (combus->digitalBus && combus->digitalBus[id].value);
			if (v) {
				packed |= (uint8_t)(1u << bit);
			}
		}

		outputBuffer[pos++] = packed;
	}

		// 6. Analog values (uint16_t little-endian)
	for (uint8_t a = 0; a < nAnalog; ++a) {
		const uint8_t id = analogMap ? analogMap[a] : a;
		uint16_t val = combus->analogBus ? combus->analogBus[id].value : 0;
		outputBuffer[pos++] = (uint8_t)(val & 0xFFu);
		outputBuffer[pos++] = (uint8_t)((val >> 8u) & 0xFFu);
	}

		// 7. CRC over everything before it
	outputBuffer[pos] = combus_frame_crc8(outputBuffer, pos);
	pos++;

	return pos;
}



// =============================================================================
// 3. DECODE
// =============================================================================

bool combus_frame_decode( const ComBusFrameCfg& cfg,
                          ComBusFrame*          outputFrame,
                          const uint8_t*        inputBuffer,
                          uint8_t               len ) {

	const uint8_t analogBufSize  = cfg.nAnalog;
	const uint8_t digitalBufSize = cfg.nDigital;

		// 1. Minimum length and SOF
	if (!outputFrame || !inputBuffer || len < CombusFrameMinLen) {return false;}
	if (inputBuffer[0] != CombusFrameSof) {return false;}

		// 2. Parse the header
	CombusFrameHeader header;

	memcpy(&header, inputBuffer + 1u, sizeof(header));   // skip SOF byte at offset 0

	uint8_t nDigBytes = (header.cfg.nDigital + 7u) / 8u;   // derive packed byte count locally

		// 3. Sanity-check the declared sizes
	if (!outputFrame->analog || !outputFrame->digital)  { return false; }

			// The frame length must fit a uint8_t
	uint16_t expectedLenW = CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)header.cfg.nAnalog * 2u + 1u;
	if (expectedLenW > 255u)                { return false; }

			// A frame larger than the set of the link (= the caller's buffers) is refused
	if (header.cfg.nAnalog  > analogBufSize)    { return false; }
	if (header.cfg.nDigital > digitalBufSize)   { return false; }

	uint8_t expectedLen = (uint8_t)expectedLenW;
	if (len < expectedLen)                  { return false; }

		// 4. Validate the CRC
	uint8_t crcExpected = inputBuffer[expectedLen - 1u];
	uint8_t crcActual   = combus_frame_crc8(inputBuffer, (uint8_t)(expectedLen - 1u));
	if (crcActual != crcExpected)           { return false; }

		// 5. Unpack the digital bits announced: the padding of the last byte never reaches the buffer
	const uint8_t nDigital = header.cfg.nDigital;

	uint8_t pos = CombusFrameHeaderLen;
	for (uint8_t b = 0; b < nDigBytes; ++b) {
		uint8_t packed = inputBuffer[pos++];
		for (uint8_t bit = 0; bit < 8u; ++bit) {
			uint8_t ch = (uint8_t)(b * 8u + bit);
			if (ch < nDigital) {
				outputFrame->digital[ch] = (packed >> bit) & 0x01u;
			}
		}
	}

		// 6. Unpack the analog values (uint16_t LE)
	for (uint8_t a = 0; a < header.cfg.nAnalog; ++a) {
		uint16_t lo  = inputBuffer[pos++];
		uint16_t hi  = inputBuffer[pos++];
		outputFrame->analog[a] = (uint16_t)(lo | (hi << 8u));
	}

		// 7. Populate the frame header
	outputFrame->header = header;

	return true;
}



// =============================================================================
// 4. APPLY
// =============================================================================

void combus_frame_apply( const ComBusFrameCfg& cfg,
                          ComBus*               combus,
                          const ComBusFrame*    inputFrame,
                          ChanLayer             caller,
                          const ComPeerLink*    peer ) {

		// Upper clamp of the channel counts
	const uint8_t nAnalog  = cfg.nAnalog;    // analog channels bus capacity
	const uint8_t nDigital = cfg.nDigital;   // digital channels bus capacity

		// 1. Guard conditions
	if (!combus || !inputFrame) {
		return;
	}

		// 2. Timestamp of the last frame
	combus->lastFrameMs = millis();   // used by link liveness checks

		// 3. Analog channels
	uint8_t nAnalogEff = (inputFrame->header.cfg.nAnalog < nAnalog) ? inputFrame->header.cfg.nAnalog : nAnalog;

			// Without a table, a channel is written only if its direction on this link
			// includes Downlink: flow.betweenNodes on an inter-node link (BASE identity),
			// flow.betweenBoards on an intra-node link.
	const bool interNode = (caller == ChanLayer::BASE);
	auto receives = [interNode](const ChanFlow& flow) {
		const Direction d = interNode ? flow.betweenNodes : flow.betweenBoards;
		return (static_cast<uint8_t>(d) & static_cast<uint8_t>(Direction::Downlink)) != 0u;
	};

			// With a table, it says which channel a wire position is, and whether this
			// node receives it.
	if (combus->analogBus) {
		for (uint8_t i = 0; i < nAnalogEff; ++i) {
			if (peer ? !peer->analog.rx[i] : !receives(combus->analogBus[i].flow)) { continue; }
			const uint8_t id = peer ? peer->analog.ids[i] : i;

				// The scope check is done by combus_set_raw_analog()
			combus_set_raw_analog(*combus, (AnalogComBusID)id, inputFrame->analog[i], caller);
		}
	}

		// 4. Digital channels
	uint8_t nDigitalEff = (inputFrame->header.cfg.nDigital < nDigital) ? inputFrame->header.cfg.nDigital : nDigital;

	if (combus->digitalBus) {
		for (uint8_t i = 0; i < nDigitalEff; ++i) {
			const uint8_t id = peer ? peer->digital.ids[i] : i;

				// both_or: never overwritten by a frame.  The received value stays in
				// the link snapshot and com_rx_update() merges it with this board's
				// own value each cycle (anti ping-pong, see protocol/both_or.md).
			if (combus->digitalBus[id].bothOr.enabled) { continue; }
			if (peer ? !peer->digital.rx[i] : !receives(combus->digitalBus[i].flow)) { continue; }

				// The scope check is done by combus_set_digital()
			combus_set_digital(*combus, (DigitalComBusID)id, inputFrame->digital[i], caller);
		}
	}
}

// EOF combus_frame.cpp
