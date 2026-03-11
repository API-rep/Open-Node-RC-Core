/******************************************************************************
 * @file sound_uart_tx.cpp
 * Sound module — machine-side UART transmitter implementation.
 *****************************************************************************/

#include "sound_uart_tx.h"

#include <Arduino.h>

#include <defs/defs.h>
#include <core/combus/combus_frame.h>
#include <core/config/outputs/outputs.h>
#include <core/system/debug/debug.h>

#ifdef SOUND_OUTPUT_UART

// Env id embedded in the frame header: derived directly from MACHINE_TYPE (CombusLayout enum).
// MACHINE_TYPE is always defined at compile time by the machine platformio env — no fallback needed.
#define SOUND_TRANSPORT_ENV_ID  static_cast<uint8_t>(CombusLayout::MACHINE_TYPE)
// SoundTransportNAnalog, SoundTransportNDigital, SoundTransportFrameSize — from outputs/sound_uart.h.


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Serial port used for the sound link (Serial2 by default on machine ESP32).
static HardwareSerial* s_serial = &Serial2;

/// Rolling sequence counter (0-255, wraps).
static uint8_t s_seq = 0u;

/// Timestamp of last transmitted frame (ms).
static uint32_t s_lastTxMs = 0u;

/// Transmit period in milliseconds — computed in sound_uart_tx_init() from board SoundTransportTxHz.
/// Zero until init is called; update() is a no-op while it remains 0.
static uint32_t s_txPeriodMs = 0u;


// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * Initialize the HardwareSerial port for the sound UART link.
 */
void sound_uart_tx_init(int txPin, int rxPin, uint32_t baud, uint32_t txHz) {
    s_txPeriodMs = 1000u / txHz;   // txHz validity guaranteed by board static_assert
    s_serial->begin(baud, SERIAL_8N1, rxPin, txPin);

#ifdef DEBUG_SYSTEM
    sys_log_info("[SOUND_TX] init — baud=%u  tx=%d  rx=%d  rate=%uHz\n",
                 baud, txPin, rxPin, txHz);
#endif
}


// =============================================================================
// 3. TRANSMIT UPDATE
// =============================================================================

/**
 * Timer-gated transmit — sends one frame per TX_PERIOD_MS.
 */
void sound_uart_tx_update(const ComBus& bus, bool failSafe) {
    if (s_txPeriodMs == 0u) {
      return;   // guard: init not called yet, or called with invalid txHz = 0 (should be caught by static_assert)
    }

    uint32_t now = millis();
    if ((now - s_lastTxMs) < s_txPeriodMs) {
        return;
    }

    s_lastTxMs = now;

      // --- Encode frame ---
    uint8_t frame[SoundTransportFrameSize];
    uint8_t frameLen = combus_frame_encode(
        frame,
        &bus,
        SoundTransportNAnalog,
        SoundTransportNDigital,
        SOUND_TRANSPORT_ENV_ID,
        s_seq,
        failSafe
    );

    if (frameLen == 0u) {
        return;
    }

      // --- Send ---
    s_serial->write(frame, frameLen);
    s_seq++;

    output_log_dbg("[SOUND_TX] seq=%u  len=%u  rl=%d  flags=0x%02X\n",
                   (unsigned)(s_seq - 1u),
                   (unsigned)frameLen,
                   (int)bus.runLevel,
                   (unsigned)(failSafe ? COMBUS_FLAG_FAILSAFE : 0u) );
}

#endif // SOUND_OUTPUT_UART

// EOF sound_uart_tx.cpp
