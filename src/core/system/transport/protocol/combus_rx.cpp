/******************************************************************************
 * @file combus_rx.cpp
 * @brief ComBus receiver — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_rx.h"

#include <stddef.h>
#include <Arduino.h>

#include <core/combus/combus_frame.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Upper bound for any single ComBus frame (transport-independent).
static constexpr uint8_t kFrameMaxLen = 255u;

/// Internal receive ring buffer backing memory.
static constexpr uint8_t kRxBufSize  = kFrameMaxLen;
static uint8_t            s_rxBuf[kRxBufSize];

struct CombusRxState {
	NodeCom* com        = nullptr;  ///< active transport interface
	uint8_t         analogBufSize    = 0u;       ///< analog buffer size (from caller)
	uint8_t         digitalBufSize   = 0u;       ///< digital buffer size (from caller)
	uint8_t         rxHead           = 0u;       ///< ring buffer read index
	uint8_t         rxCount          = 0u;       ///< bytes currently in ring buffer
	ComBusFrame     snap             = {};       ///< latest decoded snapshot
	bool            snapValid        = false;    ///< true once at least one frame decoded
	uint32_t        lastRxMs         = 0u;       ///< millis() at last successful decode
	bool            everReceived     = false;    ///< true after first valid frame received
};

static CombusRxState s_rx;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/** Append one byte to the ring buffer. Drops oldest byte on overflow. */
static void rxBufPush(uint8_t byte) {
	if (s_rx.rxCount < kRxBufSize) {
		uint8_t idx    = (uint8_t)((s_rx.rxHead + s_rx.rxCount) % kRxBufSize);
		s_rxBuf[idx]   = byte;
		s_rx.rxCount++;
	} else {
			// Overflow: drop oldest byte by advancing head
		s_rxBuf[s_rx.rxHead] = byte;
		s_rx.rxHead = (uint8_t)((s_rx.rxHead + 1u) % kRxBufSize);
	}
}

/** Return byte at logical index i (0 = oldest). */
static uint8_t rxBufAt(uint8_t i) {
	return s_rxBuf[(s_rx.rxHead + i) % kRxBufSize];
}

/** Discard the n oldest bytes. */
static void rxBufConsume(uint8_t n) {
	if (n > s_rx.rxCount) { n = s_rx.rxCount; }
	s_rx.rxHead  = (uint8_t)((s_rx.rxHead + n) % kRxBufSize);
	s_rx.rxCount = (uint8_t)(s_rx.rxCount - n);
}

/**
 * @brief Attempt to decode a valid frame from the head of the ring buffer.
 *
 * @return Number of bytes consumed, or 0 if no complete frame is ready.
 */
static uint8_t tryDecode() {

		// --- 1. Scan for SOF ---
	while (s_rx.rxCount > 0u && rxBufAt(0u) != CombusFrameSof) {
		rxBufConsume(1u);
	}

	if (s_rx.rxCount < CombusFrameMinLen) {
		return 0u;
	}

		// --- 2. Peek header to compute expected frame size ---
	if (s_rx.rxCount < CombusFrameHeaderLen) {
		return 0u;
	}
	uint8_t nAnalog   = rxBufAt(1u + offsetof(CombusFrameHeader, nAnalog));
	uint8_t nDigital  = rxBufAt(1u + offsetof(CombusFrameHeader, nDigital));
	uint8_t nDigBytes = (nDigital + 7u) / 8u;

	uint16_t expectedLenW = CombusFrameHeaderLen
	                      + (uint16_t)nDigBytes
	                      + (uint16_t)nAnalog * 2u
	                      + 1u;
	if (expectedLenW > kFrameMaxLen) {
			// Impossible frame size — discard SOF and re-sync
		rxBufConsume(1u);
		return 0u;
	}
	uint8_t expectedLen = (uint8_t)expectedLenW;
	if (s_rx.rxCount < expectedLen) {
		return 0u;
	}

		// --- 3. Copy frame into a linear buffer and decode ---
	uint8_t linear[kFrameMaxLen];
	for (uint8_t i = 0u; i < expectedLen; ++i) {
		linear[i] = rxBufAt(i);
	}

		// CRC validated before any write — failed decode leaves s_rx.snap untouched.
	if (combus_frame_decode(&s_rx.snap, linear, expectedLen,
	                        s_rx.analogBufSize, s_rx.digitalBufSize)) {
		s_rx.snapValid    = true;
		s_rx.lastRxMs     = millis();
		s_rx.everReceived = true;
		rxBufConsume(expectedLen);
		return expectedLen;
	} else {
			// CRC mismatch — discard SOF and re-sync
		rxBufConsume(1u);
		return 0u;
	}
}


// =============================================================================
// 3. PUBLIC API
// =============================================================================

void combus_rx_init( NodeCom* com,
                     uint16_t*       analogBuf,
                     uint8_t         analogBufSize,
                     bool*           digitalBuf,
                     uint8_t         digitalBufSize ) {

	if (!com || !analogBuf || !digitalBuf) { return; }

	s_rx.com       = com;
	s_rx.analogBufSize   = analogBufSize;
	s_rx.digitalBufSize  = digitalBufSize;

		// Wire caller-provided buffers into the snapshot struct.
	s_rx.snap.analog     = analogBuf;
	s_rx.snap.digital    = digitalBuf;

		// Reset ring buffer and link state.
	s_rx.rxHead       = 0u;
	s_rx.rxCount      = 0u;
	s_rx.snapValid    = false;
	s_rx.everReceived = false;

	sys_log_info("[COMBUS_RX] init — transport='%s'  A%u+D%u\n",
	             com->name,
	             (unsigned)analogBufSize, (unsigned)digitalBufSize);
}


void combus_rx_update() {
	if (!s_rx.com) { return; }

		// --- Drain available bytes into ring buffer ---
	while (s_rx.com->available(s_rx.com->ctx) > 0) {
		int b = s_rx.com->readByte(s_rx.com->ctx);
		if (b >= 0) { rxBufPush((uint8_t)b); }
	}

		// --- Try to decode (may process multiple back-to-back frames) ---
	while (s_rx.rxCount >= CombusFrameMinLen) {
		if (tryDecode() == 0u) {
			break;
		}
	}
}


const ComBusFrame* combus_rx_snapshot() {
	return s_rx.snapValid ? &s_rx.snap : nullptr;
}


uint32_t combus_rx_age_ms() {
	if (!s_rx.everReceived) {
		return UINT32_MAX;
	}
	return (uint32_t)(millis() - s_rx.lastRxMs);
}


bool combus_rx_is_alive(uint32_t timeoutMs) {
	return s_rx.everReceived && (combus_rx_age_ms() < timeoutMs);
}

// EOF combus_rx.cpp
