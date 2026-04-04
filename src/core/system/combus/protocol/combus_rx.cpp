/******************************************************************************
 * @file combus_rx.cpp
 * @brief ComBus receiver — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_rx.h"

#include <stddef.h>
#include <Arduino.h>

#include <core/system/combus/frame/combus_frame.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/**
 * Ring buffer — raw byte accumulator between the transport ISR and the frame decoder.
 * Sized to `UINT8_MAX` (255): the protocol length field is a `uint8_t`, so no
 * single frame can ever exceed that. `rxBufSize` and `frameMaxLen` are kept as
 * separate constants so each use site reads with its own intent.
 */

	// buffer sizing
static constexpr uint8_t frameMaxLen = UINT8_MAX;    ///< max encodable frame size
static constexpr uint8_t rxBufSize   = frameMaxLen;  ///< ring buffer capacity

	// ring buffer instance
static uint8_t rxBuf[rxBufSize];  ///< raw incomming storage ring buffer


/**
 * @brief Persistent state of the ComBus receiver module.
 *
 * @details Filled once by `combus_rx_init()` and updated every cycle by
 *   `combus_rx_update()`. Holds the transport interface, the static frame
 *   layout, the ring buffer state (head + count), and the decoded snapshot
 *   with its validity flags.
 *
 *   The analog and digital pointers inside `snap` are wired to the
 *   caller-provided buffers at init time and never reallocated.
 *
 *   Lifetime: static — valid for the entire program run after init.
 */

struct CombusRxState {
	NodeCom*        nodeCom      = nullptr;   ///< active transport interface
	ComBusFrameCfg  frameCfg     = {};        ///< static layout descriptor (buffer capacities)
	uint8_t         rxHead       = 0u;        ///< ring buffer read index
	uint8_t         rxCount      = 0u;        ///< bytes currently in ring buffer
	ComBusFrame     snap         = {};        ///< latest decoded snapshot
	bool            snapValid    = false;     ///< true once at least one frame decoded
	uint32_t        lastRxMs     = 0u;        ///< millis() at last successful decode
	bool            everReceived = false;     ///< true after first valid frame received
};

static CombusRxState comBusRx;  ///< Single receiver instance (one ComBus RX per node)



// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/** 
 * @brief Append one byte to the ring buffer. Drops oldest byte on overflow.
 */

static void rxBufPush(uint8_t byte) {
	if (comBusRx.rxCount < rxBufSize) {
		uint8_t idx    = (uint8_t)((comBusRx.rxHead + comBusRx.rxCount) % rxBufSize);
		rxBuf[idx]   = byte;
		comBusRx.rxCount++;
	}
	
	else {
			// Overflow: drop oldest byte by advancing head
		rxBuf[comBusRx.rxHead] = byte;
		comBusRx.rxHead = (uint8_t)((comBusRx.rxHead + 1u) % rxBufSize);
	}
}


/** 
 * @brief Return byte at logical index i (0 = oldest).
 */

static uint8_t rxBufAt(uint8_t i) {
	return rxBuf[(comBusRx.rxHead + i) % rxBufSize];
}


/** 
 * @brief Discard the n oldest bytes from the ring buffer.
 */

static void rxBufConsume(uint8_t n) {
	if (n > comBusRx.rxCount) { n = comBusRx.rxCount; }
	comBusRx.rxHead  = (uint8_t)((comBusRx.rxHead + n) % rxBufSize);
	comBusRx.rxCount = (uint8_t)(comBusRx.rxCount - n);
}




/**
 * @brief Attempt to decode a valid frame from the head of the ring buffer.
 *
 * @details Decoding sequence:
 *   1. Scan for SOF — discards leading bytes until `CombusFrameSof` is found
 *      or the buffer is exhausted.
 *   2. Peek header — reads `nAnalog` and `nDigital` from the wire header to
 *      compute the expected frame length; discards SOF and re-syncs if the
 *      size would exceed `frameMaxLen`.
 *   3. Copy and decode — extracts the complete frame into a linear buffer and
 *      calls `combus_frame_decode()`. On CRC success, updates the snapshot and
 *      consumes the frame bytes. On CRC failure, discards only the SOF and
 *      re-syncs.
 *
 * @return Number of bytes consumed, or 0 if no complete valid frame was found.
 */

static uint8_t tryDecode() {

		// --- 1. Scan for SOF ---
	while (comBusRx.rxCount > 0u && rxBufAt(0u) != CombusFrameSof) {
		rxBufConsume(1u);
	}

	if (comBusRx.rxCount < CombusFrameMinLen) {
		return 0u;
	}

		// --- 2. Peek header to compute expected frame size ---
	if (comBusRx.rxCount < CombusFrameHeaderLen) {
		return 0u;
	}
	uint8_t nAnalog   = rxBufAt(1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog));
	uint8_t nDigital  = rxBufAt(1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital));
	uint8_t nDigBytes = (nDigital + 7u) / 8u;

	uint16_t expectedLenW = CombusFrameHeaderLen
	                      + (uint16_t)nDigBytes
	                      + (uint16_t)nAnalog * 2u
	                      + 1u;
	if (expectedLenW > frameMaxLen) {
			// Impossible frame size — discard SOF and re-sync
		rxBufConsume(1u);
		return 0u;
	}
	uint8_t expectedLen = (uint8_t)expectedLenW;
	if (comBusRx.rxCount < expectedLen) {
		return 0u;
	}

		// --- 3. Copy frame into a linear buffer and decode ---
	uint8_t linear[frameMaxLen];
	for (uint8_t i = 0u; i < expectedLen; ++i) {
		linear[i] = rxBufAt(i);
	}

		// CRC validated before any write — failed decode leaves comBusRx.snap untouched.
	if (combus_frame_decode(comBusRx.frameCfg, &comBusRx.snap, linear, expectedLen)) {
		comBusRx.snapValid    = true;
		comBusRx.lastRxMs     = millis();
		comBusRx.everReceived = true;
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

/**
 * @brief Initialize the ComBus receiver.
 *
 * @details Initialization sequence:
 *   1. Guard check — null transport or buffer pointer; returns immediately.
 *   2. Store transport interface and frame layout descriptor.
 *   3. Wire caller-provided buffers into the snapshot struct.
 *   4. Reset ring buffer and link state.
 */

void combus_rx_init(
    NodeCom*       nodeCom,     // claimed transport interface (from uart_com_init or similar)
    ComBusFrameCfg frameCfg,    // static frame layout descriptor (envId, nAnalog, nDigital)
    uint16_t*      analogBuf,   // caller-allocated array of frameCfg.nAnalog entries
    bool*          digitalBuf ) // caller-allocated array of frameCfg.nDigital entries
{
		// --- 1. Guard check ---
	if (!nodeCom || !analogBuf || !digitalBuf) { return; }

		// --- 2. Store transport interface and frame layout ---
	comBusRx.nodeCom  = nodeCom;
	comBusRx.frameCfg = frameCfg;

		// --- 3. Wire caller-provided buffers into the snapshot struct ---
	comBusRx.snap.analog  = analogBuf;
	comBusRx.snap.digital = digitalBuf;

		// --- 4. Reset ring buffer and link state ---
	comBusRx.rxHead       = 0u;
	comBusRx.rxCount      = 0u;
	comBusRx.snapValid    = false;
	comBusRx.everReceived = false;

	sys_log_info("[COMBUS_RX] init — transport='%s'  A%u+D%u\n",
	             nodeCom->name,
	             (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital);
}



/**
 * @brief Poll transport and decode incoming frames — call every loop iteration.
 *
 * @details Timer-gated, non-blocking — safe to call every loop:
 *   1. Guard check — returns immediately if uninit.
 *   2. Drain available bytes from the transport into the ring buffer.
 *   3. Try to decode frames until the buffer holds fewer than `CombusFrameMinLen`
 *      bytes or `tryDecode()` finds nothing to consume.
 */

void combus_rx_update() {
		// --- 1. Guard check ---
	if (!comBusRx.nodeCom) { return; }

		// --- 2. Drain available bytes into ring buffer ---
	while (comBusRx.nodeCom->available(comBusRx.nodeCom->ctx) > 0) {
		int b = comBusRx.nodeCom->readByte(comBusRx.nodeCom->ctx);
		if (b >= 0) { rxBufPush((uint8_t)b); }
	}

		// --- 3. Try to decode (may process multiple back-to-back frames) ---
	while (comBusRx.rxCount >= CombusFrameMinLen) {
		if (tryDecode() == 0u) {
			break;
		}
	}
}



/** @brief Return latest valid snapshot, or nullptr if no frame received yet. */

const ComBusFrame* combus_rx_snapshot() {
	return comBusRx.snapValid ? &comBusRx.snap : nullptr;
}



/** @brief Milliseconds elapsed since the last valid frame, or UINT32_MAX if never received. */

uint32_t combus_rx_age_ms() {
	if (!comBusRx.everReceived) {
		return UINT32_MAX;
	}
	return (uint32_t)(millis() - comBusRx.lastRxMs);
}



/** @brief True if a valid frame was received within the last timeoutMs milliseconds. */

bool combus_rx_is_alive(uint32_t timeoutMs) {
	return comBusRx.everReceived && (combus_rx_age_ms() < timeoutMs);
}

// EOF combus_rx.cpp
