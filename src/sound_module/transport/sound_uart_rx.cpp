/******************************************************************************
 * @file sound_uart_rx.cpp
 * Sound module — sound-side UART receiver implementation.
 *****************************************************************************/

#include "sound_uart_rx.h"

#include <Arduino.h>

#include <core/utils/combus/combus_frame.h>
#include "../config/sound_config.h"


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Serial port for the machine link (Serial2 by default on sound ESP32).
static HardwareSerial* s_serial = &Serial2;

/// UART transport physical cap — upper bound for any frame arriving on this link.
static constexpr uint8_t kUartFrameMaxLen = 255u;

/// Internal receive ring buffer — large enough for several max-size frames.
static constexpr uint8_t RX_BUF_SIZE = (uint8_t)(kUartFrameMaxLen);
static uint8_t  s_rxBuf[RX_BUF_SIZE];
static uint8_t  s_rxHead = 0u;   ///< Write index (next byte goes here)
static uint8_t  s_rxCount = 0u;  ///< Number of bytes currently in buffer

/// Backing arrays for the snapshot — sized exactly to this application's channel counts.
static uint16_t       s_snapAnalog[SOUND_TRANSPORT_N_ANALOG];
static bool           s_snapDigital[SOUND_TRANSPORT_N_DIGITAL];

/// Latest decoded snapshot.
static ComBusFrame    s_snap = { .analog = s_snapAnalog, .digital = s_snapDigital };
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
    while (s_rxCount > 0u && rxBufAt(0u) != COMBUS_FRAME_SOF) {
        rxBufConsume(1u);
    }

    if (s_rxCount < COMBUS_FRAME_MIN_LEN) {
        return 0u;  // not enough bytes yet
    }

      // --- 2. Peek header to determine expected frame size ---
    //  buf[5] = nAnalog, buf[6] = nDigBytes
    if (s_rxCount < 7u) {
        return 0u;
    }
    uint8_t nAnalog   = rxBufAt(5u);
    uint8_t nDigBytes = rxBufAt(6u);

    uint16_t expectedLenW = 7u + (uint16_t)nDigBytes + (uint16_t)nAnalog * 2u + 1u;
    if (expectedLenW > kUartFrameMaxLen) {
          // Impossible frame size — discard SOF and re-sync
        rxBufConsume(1u);
        return 0u;
    }
    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (s_rxCount < expectedLen) {
        return 0u;  // frame not complete yet
    }

      // --- 3. Copy frame into a linear buffer and decode ---
    uint8_t linear[kUartFrameMaxLen];
    for (uint8_t i = 0u; i < expectedLen; ++i) {
        linear[i] = rxBufAt(i);
    }

      // Decode directly into s_snap — CRC is validated before any write,
      // so a failed decode leaves s_snap untouched.
    if (combus_frame_decode(&s_snap, linear, expectedLen,
                                  SOUND_TRANSPORT_N_ANALOG, SOUND_TRANSPORT_N_DIGITAL)) {
          // Valid frame
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
    s_rxHead         = 0u;
    s_rxCount        = 0u;
    s_snapValid      = false;
    s_everReceived   = false;
    s_snap.analog  = s_snapAnalog;
    s_snap.digital = s_snapDigital;
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
    while (s_rxCount >= COMBUS_FRAME_MIN_LEN) {
        if (tryDecode() == 0u) {
            break;
        }
    }
}

/**
 * Return latest valid snapshot or nullptr.
 */
const ComBusFrame* sound_uart_rx_snapshot() {
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
