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

/**
 * @brief Frame integrity check. Last byte of every frame, recomputed by the receiver.
 *
 * @details CRC-8/MAXIM: polynomial 0x31 (reflected 0x8C), init 0x00, bits taken LSB
 *   first.  For each bit, the low bit of the CRC is mixed with the data bit; when they
 *   differ, the polynomial is applied after the shift.  Computed bit by bit: no lookup
 *   table is needed at the frame sizes used here.
 */

uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len) {
	uint8_t crc = 0x00;

		// Mix each byte of the buffer
	for (uint8_t i = 0; i < len; ++i) {
		uint8_t byte = data[i];

			// Each bit of the byte, LSB first
		for (uint8_t b = 0; b < 8u; ++b) {
				// Compare the low bit of the CRC with the data bit, then shift the CRC
			uint8_t mix = (crc ^ byte) & 0x01u;
			crc >>= 1u;

				// Apply the polynomial when they differ
			if (mix) {
				crc ^= 0x8Cu;
			}

				// Next data bit
			byte >>= 1u;
		}
	}

	return crc;
}



// =============================================================================
// 2. ENCODE
// =============================================================================

/**
 * @brief Control frame builder. Turns the bus state into the bytes one link sends.
 *
 * @details In order:
 *   1. Counts: the channels the link asks for (`analogWireEnd`, `digitalWireEnd`),
 *      clamped to the layout `cfg`.
 *   2. Guards: nothing is written and 0 is returned on a null pointer, on `seq == 0`,
 *      or if the frame would exceed the 255 bytes a one-byte length can hold.
 *   3. Flags: no bit is defined on a control frame, the byte stays 0.
 *   4. Fixed header: SOF, counts, seq, flags.
 *   5. Digital values, 8 per byte, LSB first.  Each one comes from `digitalValues` when
 *      given (values prepared for this link), otherwise from the bus; a bus without a
 *      digital array sends false.
 *   6. Analog values, little-endian.  A bus without an analog array sends 0.
 *   7. CRC over everything before it.  The return value is the total length.
 *
 *   In steps 5 and 6, a table (`digitalMap`, `analogMap`) turns a wire position into a
 *   channel ID on a link between nodes.  Without a table, both are the same.
 */

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

			// Set the bits of this byte
		for (uint8_t bit = 0; bit < 8u; ++bit) {
			uint8_t ch = (uint8_t)(b * 8u + bit);

				// Check for the padding of the last byte: no channel behind it
			if (ch >= nDigital) { continue; }

				// The values prepared for this link (both_or without echo) when given,
				// the bus values otherwise (through the table of a link between nodes).
			const uint8_t id = digitalMap ? digitalMap[ch] : ch;
			const bool v = digitalValues ? digitalValues[ch]
			                             : (combus->digitalBus && combus->digitalBus[id].value);

				// Set the bit of a true channel
			if (v) {
				packed |= (uint8_t)(1u << bit);
			}
		}

			// Store the packed byte
		outputBuffer[pos++] = packed;
	}

		// 6. Analog values (uint16_t little-endian)
	for (uint8_t a = 0; a < nAnalog; ++a) {
			// Value of the channel at this wire position
		const uint8_t id = analogMap ? analogMap[a] : a;
		uint16_t val = combus->analogBus ? combus->analogBus[id].value : 0;

			// Store it, low byte first
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

/**
 * @brief Control frame reader. Validates the received bytes, then unpacks them.
 *
 * @details In order:
 *   1. Minimum length and SOF.
 *   2. Header, copied as is: its first two bytes are the layout of this frame.
 *   3. Declared sizes, against the 255-byte limit, the capacities in `cfg` and the
 *      bytes received.  Too few bytes is a refusal: the RX path waits for the whole
 *      frame before calling.
 *   4. CRC over everything before it.
 *   5. Digital values: only the announced bits, never the padding of the last byte.
 *   6. Analog values, little-endian.
 *   7. Header copied to the output frame.
 *
 *   Nothing is written to `outputFrame` before step 5: a refused frame leaves it
 *   untouched.  The values stay indexed by wire position: the table and the scope only
 *   come into play in combus_frame_apply().
 */

bool combus_frame_decode( const ComBusFrameCfg& cfg,
                          ComBusFrame*          outputFrame,
                          const uint8_t*        inputBuffer,
                          uint8_t               len ) {

	const uint8_t analogBufSize  = cfg.nAnalog;
	const uint8_t digitalBufSize = cfg.nDigital;

		// 1. Minimum length and SOF
	if (!outputFrame || !inputBuffer || len < CombusFrameMinLen) { return false; }
	if (inputBuffer[0] != CombusFrameSof)                        { return false; }

		// 2. Parse the header
	CombusFrameHeader header;

	memcpy(&header, inputBuffer + 1u, sizeof(header));   // skip SOF byte at offset 0

	uint8_t nDigBytes = (header.cfg.nDigital + 7u) / 8u;   // derive packed byte count locally

		// 3. Sanity-check the declared sizes
	if (!outputFrame->analog || !outputFrame->digital) { return false; }

		// The frame length must fit a uint8_t
	uint16_t expectedLenW = CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)header.cfg.nAnalog * 2u + 1u;

	if (expectedLenW > 255u) { return false; }

		// A frame larger than the set of the link (= the caller's buffers) is refused
	if (header.cfg.nAnalog  > analogBufSize)  { return false; }
	if (header.cfg.nDigital > digitalBufSize) { return false; }

		// Check that the whole frame was received
	uint8_t expectedLen = (uint8_t)expectedLenW;

	if (len < expectedLen) { return false; }

		// 4. Validate the CRC
	uint8_t crcExpected = inputBuffer[expectedLen - 1u];
	uint8_t crcActual   = combus_frame_crc8(inputBuffer, (uint8_t)(expectedLen - 1u));

	if (crcActual != crcExpected) { return false; }

		// 5. Unpack the digital bits announced: the padding of the last byte never reaches the buffer
	const uint8_t nDigital = header.cfg.nDigital;

		// The payload starts after the header
	uint8_t pos = CombusFrameHeaderLen;

		// Each packed byte
	for (uint8_t b = 0; b < nDigBytes; ++b) {
		uint8_t packed = inputBuffer[pos++];

			// Each bit of the byte, LSB first
		for (uint8_t bit = 0; bit < 8u; ++bit) {
			uint8_t ch = (uint8_t)(b * 8u + bit);

				// Check for the padding: it stays out of the buffer
			if (ch < nDigital) {
				outputFrame->digital[ch] = (packed >> bit) & 0x01u;
			}
		}
	}

		// 6. Unpack the analog values (uint16_t LE)
	for (uint8_t a = 0; a < header.cfg.nAnalog; ++a) {
			// Rebuild the value, low byte first
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

/**
 * @brief Frame to bus bridge. Writes to the bus the channels this node receives.
 *
 * @details In order:
 *   1. Guards: a null pointer changes nothing, not even the timestamp.
 *   2. Timestamp of the last frame, read by the liveness checks of the link.
 *   3. Analog channels, up to the smaller of the frame count and the `cfg` capacity.
 *   4. Digital channels, likewise; a both_or channel is skipped first.
 *
 *   Who receives a channel: with `peer`, the table of the link says it for each wire
 *   position and gives the channel ID.  Without it, the wire position is the channel
 *   ID, and the flow of the channel decides: `betweenNodes` when `caller` is BASE
 *   (link between nodes), `betweenBoards` otherwise (link inside a node).
 *
 *   A both_or channel is never written here: the value received stays in the snapshot
 *   of the link, and com_rx_update() merges it with the value of this board on every
 *   cycle (anti ping-pong, see protocol/both_or.md).
 */

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
			// Direction of the channel on this kind of link
		const Direction d = interNode ? flow.betweenNodes : flow.betweenBoards;

		return (static_cast<uint8_t>(d) & static_cast<uint8_t>(Direction::Downlink)) != 0u;
	};

		// With a table, it says which channel a wire position is, and whether this
		// node receives it.
	if (combus->analogBus) {
		for (uint8_t i = 0; i < nAnalogEff; ++i) {
				// Check that this node receives this position
			if (peer ? !peer->analog.rx[i] : !receives(combus->analogBus[i].flow)) { continue; }

				// Channel at this wire position
			const uint8_t id = peer ? peer->analog.ids[i] : i;

				// The scope check is done by combus_set_raw_analog()
			combus_set_raw_analog(*combus, (AnalogComBusID)id, inputFrame->analog[i], caller);
		}
	}

		// 4. Digital channels
	uint8_t nDigitalEff = (inputFrame->header.cfg.nDigital < nDigital) ? inputFrame->header.cfg.nDigital : nDigital;

		// Check that the bus has digital channels
	if (combus->digitalBus) {
		for (uint8_t i = 0; i < nDigitalEff; ++i) {
				// Channel at this wire position
			const uint8_t id = peer ? peer->digital.ids[i] : i;

				// both_or: never overwritten by a frame
			if (combus->digitalBus[id].bothOr.enabled) { continue; }

				// Check that this node receives this position
			if (peer ? !peer->digital.rx[i] : !receives(combus->digitalBus[i].flow)) { continue; }

				// The scope check is done by combus_set_digital()
			combus_set_digital(*combus, (DigitalComBusID)id, inputFrame->digital[i], caller);
		}
	}
}

// EOF combus_frame.cpp
