/******************************************************************************
 * @file combus_uart_rx.cpp
 * Generic ComBus UART receiver — implementation.
 *****************************************************************************/

#include "combus_uart_rx.h"

#include <stddef.h>
#include <Arduino.h>

#include <core/combus/combus_frame.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Active serial port (set at init).
static HardwareSerial* s_serial = nullptr;

/// UART transport physical cap — upper bound for any frame on this link.
static constexpr uint8_t kUartFrameMaxLen = 255u;

/// Internal receive ring buffer — large enough for several max-size frames.
static constexpr uint8_t RX_BUF_SIZE = kUartFrameMaxLen;
static uint8_t  s_rxBuf[RX_BUF_SIZE];
static uint8_t  s_rxHead  = 0u;   ///< Write index (next byte goes here).
static uint8_t  s_rxCount = 0u;   ///< Number of bytes currently in buffer.

/// Caller-provided buffer sizes (stored at init for decode calls).
static uint8_t s_analogBufSize  = 0u;
static uint8_t s_digitalBufSize = 0u;

/// Latest decoded snapshot (analog/digital pointers set at init to caller buffers).
static ComBusFrame s_snap      = {};
static bool        s_snapValid = false;

/// Link telemetry.
static uint32_t s_lastRxMs    = 0u;
static bool     s_everReceived = false;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/** Append one byte to the ring buffer. Drops oldest byte on overflow. */
static void rxBufPush(uint8_t byte) {
    if (s_rxCount < RX_BUF_SIZE) {
        uint8_t idx = (uint8_t)((s_rxHead + s_rxCount) % RX_BUF_SIZE);
        s_rxBuf[idx] = byte;
        s_rxCount++;
    } else {
          // Overflow: drop oldest byte by advancing head
        s_rxBuf[s_rxHead] = byte;
        s_rxHead = (uint8_t)((s_rxHead + 1u) % RX_BUF_SIZE);
    }
}

/** Return byte at logical index i (0 = oldest). */
static uint8_t rxBufAt(uint8_t i) {
    return s_rxBuf[(s_rxHead + i) % RX_BUF_SIZE];
}

/** Discard the n oldest bytes. */
static void rxBufConsume(uint8_t n) {
    if (n > s_rxCount) { n = s_rxCount; }
    s_rxHead  = (uint8_t)((s_rxHead + n) % RX_BUF_SIZE);
    s_rxCount = (uint8_t)(s_rxCount - n);
}

/**
 * Attempt to decode a valid frame from the head of the ring buffer.
 *
 * @return Number of bytes consumed, or 0 if no complete frame is ready.
 */
static uint8_t tryDecode() {

      // --- 1. Scan for SOF ---
    while (s_rxCount > 0u && rxBufAt(0u) != CombusFrameSof) {
        rxBufConsume(1u);
    }

    if (s_rxCount < CombusFrameMinLen) {
        return 0u;
    }

      // --- 2. Peek header to compute expected frame size ---
    if (s_rxCount < CombusFrameHeaderLen) {
        return 0u;
    }
    uint8_t nAnalog  = rxBufAt(1u + offsetof(CombusFrameHeader, nAnalog));
    uint8_t nDigital = rxBufAt(1u + offsetof(CombusFrameHeader, nDigital));
    uint8_t nDigBytes = (nDigital + 7u) / 8u;

    uint16_t expectedLenW = CombusFrameHeaderLen
                          + (uint16_t)nDigBytes
                          + (uint16_t)nAnalog * 2u
                          + 1u;
    if (expectedLenW > kUartFrameMaxLen) {
          // Impossible frame size — discard SOF and re-sync
        rxBufConsume(1u);
        return 0u;
    }
    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (s_rxCount < expectedLen) {
        return 0u;
    }

      // --- 3. Copy frame into a linear buffer and decode ---
    uint8_t linear[kUartFrameMaxLen];
    for (uint8_t i = 0u; i < expectedLen; ++i) {
        linear[i] = rxBufAt(i);
    }

      // CRC validated before any write — failed decode leaves s_snap untouched.
    if (combus_frame_decode(&s_snap, linear, expectedLen,
                            s_analogBufSize, s_digitalBufSize)) {
        s_snapValid    = true;
        s_lastRxMs     = millis();
        s_everReceived = true;
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

void combus_uart_rx_init( HardwareSerial* serial,
                           uint32_t        baud,
                           int             rxPin,
                           int             txPin,
                           uint16_t*       analogBuf,
                           uint8_t         analogBufSize,
                           bool*           digitalBuf,
                           uint8_t         digitalBufSize ) {

    if (!serial || !analogBuf || !digitalBuf) { return; }

    s_serial         = serial;
    s_analogBufSize  = analogBufSize;
    s_digitalBufSize = digitalBufSize;

      // Wire caller-provided buffers into the snapshot struct.
    s_snap.analog    = analogBuf;
    s_snap.digital   = digitalBuf;

      // Reset ring buffer and link state.
    s_rxHead      = 0u;
    s_rxCount     = 0u;
    s_snapValid   = false;
    s_everReceived = false;

    serial->begin(baud, SERIAL_8N1, rxPin, txPin);

    sys_log_info("[UART_RX] init — baud=%u  rx=%d  tx=%d  A%u+D%u\n",
                 baud, rxPin, txPin,
                 (unsigned)analogBufSize, (unsigned)digitalBufSize);
}


void combus_uart_rx_update() {
    if (!s_serial) { return; }

      // --- Drain available bytes into ring buffer ---
    while (s_serial->available() > 0) {
        rxBufPush((uint8_t)s_serial->read());
    }

      // --- Try to decode (may process multiple back-to-back frames) ---
    while (s_rxCount >= CombusFrameMinLen) {
        if (tryDecode() == 0u) {
            break;
        }
    }
}


const ComBusFrame* combus_uart_rx_snapshot() {
    return s_snapValid ? &s_snap : nullptr;
}


uint32_t combus_uart_rx_age_ms() {
    if (!s_everReceived) {
        return UINT32_MAX;
    }
    return (uint32_t)(millis() - s_lastRxMs);
}


bool combus_uart_rx_is_alive(uint32_t timeoutMs) {
    return s_everReceived && (combus_uart_rx_age_ms() < timeoutMs);
}

// EOF combus_uart_rx.cpp
