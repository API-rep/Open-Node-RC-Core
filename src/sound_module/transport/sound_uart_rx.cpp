/******************************************************************************
 * @file sound_uart_rx.cpp
 * Sound module — sound-side UART receiver implementation.
 *****************************************************************************/

#include "sound_uart_rx.h"

#include <Arduino.h>

#include <core/transport/combus_transport.h>
#include "../config/sound_config.h"


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Serial port for the machine link (Serial2 by default on sound ESP32).
static HardwareSerial* s_serial = &Serial2;

/// Internal receive ring buffer — large enough for two max-size frames.
static constexpr uint8_t RX_BUF_SIZE = (uint8_t)(COMBUS_TRANSPORT_MAX_FRAME * 2u);
static uint8_t  s_rxBuf[RX_BUF_SIZE];
static uint8_t  s_rxHead = 0u;   ///< Write index (next byte goes here)
static uint8_t  s_rxCount = 0u;  ///< Number of bytes currently in buffer

/// Latest decoded snapshot (double-buffered: decode into s_dec, copy to s_snap on success).
static ComBusSnapshot s_snap;
static bool           s_snapValid = false;

/// Timestamp of last valid frame reception.
static uint32_t s_lastRxMs   = 0u;
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

/** Discard the n oldest bytes from the buffer. */
static void rxBufConsume(uint8_t n) {
    if (n > s_rxCount) { n = s_rxCount; }
    s_rxHead  = (uint8_t)((s_rxHead + n) % RX_BUF_SIZE);
    s_rxCount = (uint8_t)(s_rxCount - n);
}

/**
 * Attempt to find and decode a valid frame from the head of the buffer.
 * Returns the number of bytes consumed (frame + any leading garbage), or 0
 * if no complete frame is available yet.
 */
static uint8_t tryDecode() {
      // --- 1. Scan for SOF ---
    while (s_rxCount > 0u && rxBufAt(0u) != COMBUS_TRANSPORT_SOF) {
        rxBufConsume(1u);
    }

    if (s_rxCount < COMBUS_TRANSPORT_MIN_FRAME) {
        return 0u;  // not enough bytes yet
    }

      // --- 2. Peek header to determine expected frame size ---
    //  buf[5] = nAnalog, buf[6] = nDigBytes
    if (s_rxCount < 7u) {
        return 0u;
    }
    uint8_t nAnalog   = rxBufAt(5u);
    uint8_t nDigBytes = rxBufAt(6u);

    if (nAnalog   > COMBUS_TRANSPORT_MAX_ANALOG ||
        nDigBytes > (COMBUS_TRANSPORT_MAX_DIGITAL / 8u)) {
          // Invalid header — discard SOF and re-sync
        rxBufConsume(1u);
        return 0u;
    }

    uint8_t expectedLen = (uint8_t)(7u + nDigBytes + nAnalog * 2u + 1u);
    if (s_rxCount < expectedLen) {
        return 0u;  // frame not complete yet
    }

      // --- 3. Copy frame into a linear buffer and decode ---
    uint8_t linear[COMBUS_TRANSPORT_MAX_FRAME];
    for (uint8_t i = 0u; i < expectedLen; ++i) {
        linear[i] = rxBufAt(i);
    }

    ComBusSnapshot dec;
    if (combus_transport_decode(&dec, linear, expectedLen)) {
          // Valid frame — commit to public snapshot
        s_snap      = dec;
        s_snapValid = true;
        s_lastRxMs  = millis();
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

/**
 * Initialize the UART receiver.
 */
void sound_uart_rx_init(int rxPin, int txPin) {
    s_serial->begin(SOUND_UART_BAUD, SERIAL_8N1, rxPin, txPin);
    s_rxHead       = 0u;
    s_rxCount      = 0u;
    s_snapValid    = false;
    s_everReceived = false;
}

/**
 * Non-blocking UART poll — call every loop iteration.
 */
void sound_uart_rx_update() {
      // --- Drain available bytes into ring buffer ---
    while (s_serial->available() > 0) {
        rxBufPush((uint8_t)s_serial->read());
    }

      // --- Try to decode frames (may process multiple back-to-back frames) ---
    while (s_rxCount >= COMBUS_TRANSPORT_MIN_FRAME) {
        if (tryDecode() == 0u) {
            break;
        }
    }
}

/**
 * Return latest valid snapshot or nullptr.
 */
const ComBusSnapshot* sound_uart_rx_snapshot() {
    return s_snapValid ? &s_snap : nullptr;
}

/**
 * Milliseconds since last valid frame.
 */
uint32_t sound_uart_rx_age_ms() {
    if (!s_everReceived) {
        return UINT32_MAX;
    }
    return (uint32_t)(millis() - s_lastRxMs);
}

/**
 * True if link is alive (within timeoutMs).
 */
bool sound_uart_rx_is_alive(uint32_t timeoutMs) {
    return s_everReceived && (sound_uart_rx_age_ms() < timeoutMs);
}

// EOF sound_uart_rx.cpp
