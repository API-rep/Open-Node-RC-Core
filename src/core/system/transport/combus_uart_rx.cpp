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

/// UART transport physical cap — upper bound for any frame on this link.
static constexpr uint8_t kUartFrameMaxLen = 255u;

/// Internal receive ring buffer backing memory.
static constexpr uint8_t kRxBufSize = kUartFrameMaxLen;
static uint8_t uartRxBuf[kRxBufSize];

struct UartRxState {
    HardwareSerial* port            = nullptr;  ///< active serial port
    uint8_t         analogBufSize   = 0u;       ///< analog buffer size (from caller)
    uint8_t         digitalBufSize  = 0u;       ///< digital buffer size (from caller)
    uint8_t         rxHead          = 0u;       ///< ring buffer read index
    uint8_t         rxCount         = 0u;       ///< bytes currently in ring buffer
    ComBusFrame     snap            = {};       ///< latest decoded snapshot
    bool            snapValid       = false;    ///< true once at least one frame decoded
    uint32_t        lastRxMs        = 0u;       ///< millis() at last successful decode
    bool            everReceived    = false;    ///< true after first valid frame received
};

static UartRxState uartRx;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/** Append one byte to the ring buffer. Drops oldest byte on overflow. */
static void rxBufPush(uint8_t byte) {
    if (uartRx.rxCount < kRxBufSize) {
        uint8_t idx = (uint8_t)((uartRx.rxHead + uartRx.rxCount) % kRxBufSize);
        uartRxBuf[idx] = byte;
        uartRx.rxCount++;
    } else {
          // Overflow: drop oldest byte by advancing head
        uartRxBuf[uartRx.rxHead] = byte;
        uartRx.rxHead = (uint8_t)((uartRx.rxHead + 1u) % kRxBufSize);
    }
}

/** Return byte at logical index i (0 = oldest). */
static uint8_t rxBufAt(uint8_t i) {
    return uartRxBuf[(uartRx.rxHead + i) % kRxBufSize];
}

/** Discard the n oldest bytes. */
static void rxBufConsume(uint8_t n) {
    if (n > uartRx.rxCount) { n = uartRx.rxCount; }
    uartRx.rxHead  = (uint8_t)((uartRx.rxHead + n) % kRxBufSize);
    uartRx.rxCount = (uint8_t)(uartRx.rxCount - n);
}

/**
 * Attempt to decode a valid frame from the head of the ring buffer.
 *
 * @return Number of bytes consumed, or 0 if no complete frame is ready.
 */
static uint8_t tryDecode() {

      // --- 1. Scan for SOF ---
    while (uartRx.rxCount > 0u && rxBufAt(0u) != CombusFrameSof) {
        rxBufConsume(1u);
    }

    if (uartRx.rxCount < CombusFrameMinLen) {
        return 0u;
    }

      // --- 2. Peek header to compute expected frame size ---
    if (uartRx.rxCount < CombusFrameHeaderLen) {
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
    if (uartRx.rxCount < expectedLen) {
        return 0u;
    }

      // --- 3. Copy frame into a linear buffer and decode ---
    uint8_t linear[kUartFrameMaxLen];
    for (uint8_t i = 0u; i < expectedLen; ++i) {
        linear[i] = rxBufAt(i);
    }

      // CRC validated before any write — failed decode leaves uartRx.snap untouched.
    if (combus_frame_decode(&uartRx.snap, linear, expectedLen,
                            uartRx.analogBufSize, uartRx.digitalBufSize)) {
        uartRx.snapValid     = true;
        uartRx.lastRxMs      = millis();
        uartRx.everReceived  = true;
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

    uartRx.port            = serial;
    uartRx.analogBufSize   = analogBufSize;
    uartRx.digitalBufSize  = digitalBufSize;

      // Wire caller-provided buffers into the snapshot struct.
    uartRx.snap.analog     = analogBuf;
    uartRx.snap.digital    = digitalBuf;

      // Reset ring buffer and link state.
    uartRx.rxHead       = 0u;
    uartRx.rxCount      = 0u;
    uartRx.snapValid    = false;
    uartRx.everReceived = false;

    serial->begin(baud, SERIAL_8N1, rxPin, txPin);

    sys_log_info("[UART_RX] init — baud=%u  rx=%d  tx=%d  A%u+D%u\n",
                 baud, rxPin, txPin,
                 (unsigned)analogBufSize, (unsigned)digitalBufSize);
}


void combus_uart_rx_update() {
    if (!uartRx.port) { return; }

      // --- Drain available bytes into ring buffer ---
    while (uartRx.port->available() > 0) {
        rxBufPush((uint8_t)uartRx.port->read());
    }

      // --- Try to decode (may process multiple back-to-back frames) ---
    while (uartRx.rxCount >= CombusFrameMinLen) {
        if (tryDecode() == 0u) {
            break;
        }
    }
}


const ComBusFrame* combus_uart_rx_snapshot() {
    return uartRx.snapValid ? &uartRx.snap : nullptr;
}


uint32_t combus_uart_rx_age_ms() {
    if (!uartRx.everReceived) {
        return UINT32_MAX;
    }
    return (uint32_t)(millis() - uartRx.lastRxMs);
}


bool combus_uart_rx_is_alive(uint32_t timeoutMs) {
    return uartRx.everReceived && (combus_uart_rx_age_ms() < timeoutMs);
}

// EOF combus_uart_rx.cpp
