/******************************************************************************
 * @file combus_frame.cpp
 * ComBus binary frame codec — encoding, decoding, CRC-8, and bus application.
 *
 * @details Implements frame encoding, decoding, CRC-8/MAXIM and
 * frame-to-ComBus application. The algorithm is platform-independent
 * (no Arduino or ESP-IDF calls) so it compiles on both ESP32 targets.
 *
 * LY2 — the codec now takes two counters `analogWireEnd` /
 * `digitalWireEnd` on encode.  These are resolved at TX init time
 * from the link's `ChanLayer` (REMOTE → CH_COUNT of the Remote view,
 * LOCAL → CH_COUNT of the Local view, FULL → CH_COUNT of the Full
 * view).  The codec iterates only over `comBus.analogBus[0..analogWireEnd)`
 * and `comBus.digitalBus[0..digitalWireEnd)` — this is the ONLY
 * behavioural change vs. the pre-LY2 codec.  Decode and apply are
 * layer-agnostic: they write by index into the caller-provided
 * buffers without any notion of layer.
 *****************************************************************************/

#include "combus_frame.h"

#include <string.h>
#include <Arduino.h>
#include <core/system/combus/combus_access.h>



// =============================================================================
// 1. CRC
// =============================================================================

/**
 * CRC-8/MAXIM (Dallas 1-Wire) — polynomial 0x31, init 0x00, reflect in/out.
 *
 * @details Iterative byte-by-byte computation — no lookup table needed at
 * the frame sizes used here. Safe to call from any context.
 *
 * @param[in] data  Input byte buffer.
 * @param[in] len   Number of bytes to process.
 *
 * @return CRC-8 value over the input buffer.
 */

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

/**
 * Encode a ComBus state into a binary frame written into outputBuffer.
 *
 * @details Serialization sequence:
 *   1. Null pointer and frame size overflow guard (returns 0 on failure).
 *   2. Build flags byte from transport-level inputs (failSafe, ...).
 *   3. Write fixed 5-byte header: SOF, nAnalog, nDigital, seq, flags.
 *      RL2: runLevel is no longer a header field — it travels as a
 *      standard combus channel (RUNLEVEL, scope LOCAL, see runlevel.cb).
 *      The header is now 5 bytes (was 6 before RL2).
 *   4. Pack digital channel values as bits, LSB-first, ceil(nDigital/8) bytes.
 *   5. Write analog channel values as uint16_t little-endian, nAnalog entries.
 *   6. Append CRC-8/MAXIM over all preceding bytes.
 *
 * The caller is responsible for sizing outputBuffer to at least
 * CombusFrameHeaderLen + ceil(nDigital/8) + nAnalog*2 + 1 bytes.
 *
 * LY2 — the codec iterates only over `comBus.analogBus[0..analogWireEnd)`
 * and `comBus.digitalBus[0..digitalWireEnd)`.  These two counters are
 * resolved at TX init time from the link's `ChanLayer` (REMOTE → CH_COUNT
 * of the Remote view, LOCAL → CH_COUNT of the Local view, FULL → CH_COUNT
 * of the Full view).  The wire payload is now sized to the layer's
 * prefix length instead of the full enum set.
 *
 * @param[out] outputBuffer     Destination buffer (sized by caller).
 * @param[in]  combus           Source ComBus instance to encode.
 * @param[in]  nAnalog          Number of analog channels to include.
 * @param[in]  nDigital         Number of digital channels to include.
 * @param[in]  seq              Rolling sequence counter for control frames
 *                              (1..255 — caller increments; value 0 is RESERVED
 *                              for handshake frames, see combus_handshake.h).
 * @param[in]  analogWireEnd   LY2 — number of analog channels to encode
 *                              (resolved at TX init from link->layer).
 * @param[in]  digitalWireEnd   LY2 — number of digital channels to encode
 *                              (resolved at TX init from link->layer).
 *
 * @return Number of bytes written into outputBuffer, 0 on error.
 *
 * @note FS2 — the legacy `failSafe` parameter has been removed.  The
 *   failsafe state is now carried by the `DigitalComBusID::FAILSAFE`
 *   channel (LOCAL, both_or) — no out-of-band bit needed.  See
 *   doc/WIP - Failsafe module design.md §12.13.
 */

uint8_t combus_frame_encode( const ComBusFrameCfg& cfg,
                             uint8_t*              outputBuffer,
                             const ComBus*         combus,
                             uint8_t               seq,
                             uint8_t               analogWireEnd,
                             uint8_t               digitalWireEnd ) {

		// LY2 — the wire payload is sized to the layer's prefix length.
		// analogWireEnd / digitalWireEnd are resolved at TX init time
		// from the link's ChanLayer (REMOTE → CH_COUNT of the Remote
		// view, LOCAL → CH_COUNT of the Local view, FULL → CH_COUNT of
		// the Full view).  Clamp to cfg.nAnalog / cfg.nDigital as a
		// safety net (the caller-provided buffers cannot be exceeded).
	const uint8_t nAnalog  = (analogWireEnd  < cfg.nAnalog)  ? analogWireEnd  : cfg.nAnalog;
	const uint8_t nDigital = (digitalWireEnd < cfg.nDigital) ? digitalWireEnd : cfg.nDigital;

		// 1. Guard conditions — null pointer + frame size overflow
	if (!outputBuffer || !combus) {
		return 0;
	}
		// Reject seq == 0: value 0 is RESERVED for the handshake / versioning
		// frame (see combus_handshake.h).  A control frame with seq==0 on the
		// wire would be intercepted by the handshake RX path and decoded as a
		// bogus MD5/version, causing a continuous MISMATCH storm in the logs.
		// The sole legitimate producer of seq==0 is combus_handshake_tx.cpp,
		// which builds the frame manually and never goes through this function.
	if (seq == 0u) {
		return 0;
	}
		// Reject frame size over max uint8_t size (255u) to avoid overflow
	uint8_t nDigBytes = (nDigital + 7u) / 8u;   // ceil(nDigital / 8)

	if ((CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)nAnalog * 2u + 1u) > 255u) {
		return 0;
	}

		// 2. Build flags byte
	uint8_t flags = 0;   // transport-level status bits (COMBUS_FLAG_*)

	// FS2 — COMBUS_FLAG_FAILSAFE removed.  Failsafe state is now carried
	// by the DigitalComBusID::FAILSAFE channel (LOCAL, both_or) — no
	// out-of-band bit needed.  See doc/WIP - Failsafe module design.md §12.13.
	//if (...)     { flags |= COMBUS_FLAG_... ; }       // next flag — bit 1
	//if (...)     { flags |= COMBUS_FLAG_... ; }       // next flag — bit 2

		// 3. Write fixed header
	uint8_t pos = 0;   // write position in outputBuffer

	outputBuffer[pos++] = CombusFrameSof;
	outputBuffer[pos++] = nAnalog;
	outputBuffer[pos++] = nDigital;
	outputBuffer[pos++] = seq;
		// RL2: runLevel removed from header — travels as standard combus
		// channel RUNLEVEL (scope LOCAL, see runlevel.cb). The header is
		// now 5 bytes (was 6 before RL2).
	outputBuffer[pos++] = flags;

		// 4. Pack digital bits values into bytes (bitbool lsb mode)
	for (uint8_t b = 0; b < nDigBytes; ++b) {
		uint8_t packed = 0;

		for (uint8_t bit = 0; bit < 8u; ++bit) {
			uint8_t ch = (uint8_t)(b * 8u + bit);

			if (ch < nDigital && combus->digitalBus && combus->digitalBus[ch].value) {
				packed |= (uint8_t)(1u << bit);
			}
		}

		outputBuffer[pos++] = packed;
	}

		// 5. Write analog values (uint16_t little-endian)
	for (uint8_t a = 0; a < nAnalog; ++a) {
		uint16_t val = combus->analogBus ? combus->analogBus[a].value : 0;
		outputBuffer[pos++] = (uint8_t)(val & 0xFFu);
		outputBuffer[pos++] = (uint8_t)((val >> 8u) & 0xFFu);
	}

		// 6. Append CRC8
	outputBuffer[pos] = combus_frame_crc8(outputBuffer, pos);
	pos++;

	return pos;
}



// =============================================================================
// 3. DECODE
// =============================================================================

/**
 * Decode and validate a binary frame from inputBuffer into outputFrame.
 *
 * @details Validation sequence (any failure returns false):
 *   1. Null pointer and minimum length check (>= CombusFrameMinLen).
 *   2. SOF sentinel check (inputBuffer[0] == CombusFrameSof).
 *   3. Output buffer pointer check (outputFrame->analog and ->digital must be set by caller).
 *   4. Declared payload size sanity (computed frame length must fit in uint8_t).
 *   5. Analog overflow check (header.cfg.nAnalog <= maxAnalog).
 *   6. Received length check (len >= computed expected length).
 *   7. CRC-8/MAXIM check over bytes [0 ... expectedLen-2].
 *
 * On success, unpacks digital bits (LSB-first) and analog uint16_t LE values
 * into the caller-provided arrays, then copies the header into outputFrame.
 *
 * Digital channels exceeding digitalBufSize are silently dropped (not an error).
 * outputFrame->header.cfg.nDigital reflects the clamped count after decoding.
 *
 * LY2 — decode is layer-agnostic.  It writes by index into the
 * caller-provided buffers without any notion of layer.  The layer
 * resolution happens at apply time (see combus_frame_apply).
 *
 * @param[out] outputFrame     Destination frame — analog/digital pointers must be
 *                              pre-set by the caller to buffers of at least
 *                              analogBufSize / digitalBufSize entries.
 * @param[in]  inputBuffer     Raw received bytes.
 * @param[in]  len             Number of valid bytes in inputBuffer.
 * @param[in]  analogBufSize   Capacity of outputFrame->analog[]  (caller's buffer size).
 * @param[in]  digitalBufSize  Capacity of outputFrame->digital[] (caller's buffer size).
 *
 * @return true if frame is valid and outputFrame was fully populated, false otherwise.
 */

bool combus_frame_decode( const ComBusFrameCfg& cfg,
                          ComBusFrame*          outputFrame,
                          const uint8_t*        inputBuffer,
                          uint8_t               len ) {

	const uint8_t analogBufSize  = cfg.nAnalog;
	const uint8_t digitalBufSize = cfg.nDigital;

		// 1. Minimum length and SOF guard check
	if (!outputFrame || !inputBuffer || len < CombusFrameMinLen) {return false;}
	if (inputBuffer[0] != CombusFrameSof) {return false;}

		// 2. Parse header fields
	CombusFrameHeader header;

	memcpy(&header, inputBuffer + 1u, sizeof(header));   // skip SOF byte at offset 0

	uint8_t nDigBytes = (header.cfg.nDigital + 7u) / 8u;   // derive packed byte count locally

		// 3. Sanity-check declared sizes
	if (!outputFrame->analog || !outputFrame->digital)  { return false; }

		// Reject if computed frame length overflows uint8_t.
	uint16_t expectedLenW = CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)header.cfg.nAnalog * 2u + 1u;
	if (expectedLenW > 255u)                { return false; }

		// Reject if analog payload would overflow caller's buffer.
	if (header.cfg.nAnalog > analogBufSize)     { return false; }
		// Digital excess bits are silently truncated in the unpack loop below.

	uint8_t expectedLen = (uint8_t)expectedLenW;
	if (len < expectedLen)                  { return false; }

		// 4. Validate CRC
	uint8_t crcExpected = inputBuffer[expectedLen - 1u];
	uint8_t crcActual   = combus_frame_crc8(inputBuffer, (uint8_t)(expectedLen - 1u));
	if (crcActual != crcExpected)           { return false; }

		// 5. Unpack digital bits
	uint8_t nDigital = header.cfg.nDigital;

	if (nDigital > digitalBufSize) { nDigital = digitalBufSize; }  // clamp to caller's buffer

	uint8_t pos = CombusFrameHeaderLen;
	for (uint8_t b = 0; b < nDigBytes; ++b) {
		uint8_t packed = inputBuffer[pos++];
		for (uint8_t bit = 0; bit < 8u; ++bit) {
			uint8_t ch = (uint8_t)(b * 8u + bit);
			if (ch < digitalBufSize) {
				outputFrame->digital[ch] = (packed >> bit) & 0x01u;
			}
		}
	}

		// 6. Unpack analog values (uint16_t LE)
	for (uint8_t a = 0; a < header.cfg.nAnalog; ++a) {
		uint16_t lo  = inputBuffer[pos++];
		uint16_t hi  = inputBuffer[pos++];
		outputFrame->analog[a] = (uint16_t)(lo | (hi << 8u));
	}

		// 7. Populate frame header
	outputFrame->header              = header;
	outputFrame->header.cfg.nDigital = nDigital;   // apply clamped value (may differ from wire value)

	return true;
}



// =============================================================================
// 4. APPLY FRAME → COMBUS
// =============================================================================

/**
 * Apply a decoded ComBusFrame onto a live ComBus instance.
 *
 * @details Write the incoming combus frame into the live combus instance.
 * Application sequence:
 *   1. Null pointer guard (returns on failure).
 *   2. Write runLevel from frame header; stamp combus->lastFrameMs = millis()
 *      so combus_watchdog can detect frame loss and clear isDrived.
 *   3. Reserved — transport flags (unused for now).
 *   4. Write analog channels [0 .. min(cfg.nAnalog, header.cfg.nAnalog)-1]
 *      and mark isDrived = true.
 *   5. Write digital channels [0 .. min(cfg.nDigital, header.cfg.nDigital)-1]
 *      and mark isDrived = true.
 *
 * Channels beyond the effective count are left untouched in the live bus.
 * Caller must ensure combus->analogBus and combus->digitalBus arrays are
 * allocated and sized to at least cfg.nAnalog / cfg.nDigital entries.
 *
 * LY2 — apply is layer-agnostic.  It writes by index into the
 * caller-provided ComBus arrays without any notion of layer.  The
 * layer check is delegated to combus_set_*() which reads the
 * channel's declared `layer` from the shared backing storage.
 *
 * @param[in]  cfg         Layout descriptor used as upper clamp (nAnalog, nDigital).
 * @param[out] combus      Target ComBus instance to update.
 * @param[in]  inputFrame  Populated frame from combus_frame_decode().
 * @param[in]  caller      Propagation layer forwarded to combus_set_*() guards.
 */
void combus_frame_apply( const ComBusFrameCfg& cfg,
                          ComBus*               combus,
                          const ComBusFrame*    inputFrame,
                          ChanLayer             caller ) {

		// Analog and digital channels upper clamp
	const uint8_t nAnalog  = cfg.nAnalog;    // analog channels bus capacity
	const uint8_t nDigital = cfg.nDigital;   // digital channels bus capacity

		// 1. Guard conditions — null pointer
	if (!combus || !inputFrame) {
		return;
	}

		// 2. Watchdog timestamp only.
		// RL3: runLevel propagation moved out of combus_frame_apply() and into
		// the generic analog channel RUNLEVEL (see cb_runlevel.cpp). The codec
		// stays 100% channel-agnostic — no special-cased runLevel here.
	combus->lastFrameMs = millis();   // used by sound node liveness check

		// 3. Flags (transport status only)


		// 4. Analog channels
	uint8_t nAnalogEff = (inputFrame->header.cfg.nAnalog < nAnalog) ? inputFrame->header.cfg.nAnalog : nAnalog;

	if (combus->analogBus) {
		for (uint8_t i = 0; i < nAnalogEff; ++i) {
				// LY2 — apply is layer-agnostic.  The layer check is
				// delegated to combus_set_analog() which reads the
				// channel's declared `layer` from the shared backing
				// storage.  We write by index without any notion of
				// layer here.
			combus_set_analog(*combus, (AnalogComBusID)i, inputFrame->analog[i], caller);
		}
	}

		// 5. Digital channels
	uint8_t nDigitalEff = (inputFrame->header.cfg.nDigital < nDigital) ? inputFrame->header.cfg.nDigital : nDigital;

	if (combus->digitalBus) {
		for (uint8_t i = 0; i < nDigitalEff; ++i) {
				// LY2 — see analog loop above.
			combus_set_digital(*combus, (DigitalComBusID)i, inputFrame->digital[i], caller);
		}
	}

		// 6. Mark bus as actively driven by this frame (FS1 — inverted semantics)
		// Clear isNotDrived: a valid frame was just applied (healthy).
	combus->isNotDrived = false;
}

// EOF combus_frame.cpp