/******************************************************************************
 * @file sound_uart_tx.cpp
 * Sound module — machine-side UART transmitter implementation.
 *****************************************************************************/

#include "sound_uart_tx.h"

#include <Arduino.h>

#include <core/transport/combus_transport.h>
#include <core/config/combus/combus_types.h>
#include <core/utils/debug/debug.h>

// --- TX configuration (override via build flags in platformio.ini) ---
#ifndef SOUND_UART_BAUD
  #define SOUND_UART_BAUD  115200u
#endif
#ifndef SOUND_TRANSPORT_TX_HZ
  #define SOUND_TRANSPORT_TX_HZ  50u
#endif
#ifndef SOUND_ENV_ID
  #define SOUND_ENV_ID  1u   // CombusLayout::DUMPER_TRUCK
#endif
#define SOUND_TRANSPORT_ENV_ID    ((uint8_t)(SOUND_ENV_ID))
#define SOUND_TRANSPORT_N_ANALOG  (static_cast<uint8_t>(AnalogComBusID::CH_COUNT))
#define SOUND_TRANSPORT_N_DIGITAL (static_cast<uint8_t>(DigitalComBusID::CH_COUNT))


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Serial port used for the sound link (Serial2 by default on machine ESP32).
static HardwareSerial* s_serial = &Serial2;

/// Rolling sequence counter (0-255, wraps).
static uint8_t s_seq = 0u;

/// Timestamp of last transmitted frame (ms).
static uint32_t s_lastTxMs = 0u;

/// Transmit period in milliseconds derived from configured TX rate.
static constexpr uint32_t TX_PERIOD_MS = 1000u / SOUND_TRANSPORT_TX_HZ;


// =============================================================================
// 2. INITIALIZATION
// =============================================================================

/**
 * Initialize the HardwareSerial port for the sound UART link.
 */
void sound_uart_tx_init(int txPin, int rxPin) {
    s_serial->begin(SOUND_UART_BAUD, SERIAL_8N1, rxPin, txPin);

#ifdef DEBUG_SYSTEM
    sys_log_info("[SOUND_TX] init — baud=%u  tx=%d  rx=%d  rate=%uHz\n",
                 SOUND_UART_BAUD, txPin, rxPin, SOUND_TRANSPORT_TX_HZ);
#endif
}


// =============================================================================
// 3. TRANSMIT UPDATE
// =============================================================================

/**
 * Timer-gated transmit — sends one frame per TX_PERIOD_MS.
 */
void sound_uart_tx_update(const ComBus& bus, bool failSafe) {
    uint32_t now = millis();
    if ((now - s_lastTxMs) < TX_PERIOD_MS) {
        return;
    }
    s_lastTxMs = now;

      // --- Encode frame ---
    uint8_t frame[COMBUS_TRANSPORT_MAX_FRAME];
    uint8_t frameLen = combus_transport_encode(
        frame,
        &bus,
        SOUND_TRANSPORT_N_ANALOG,
        SOUND_TRANSPORT_N_DIGITAL,
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

#if defined(DEBUG_COMBUS) && defined(DEBUG_SYSTEM)
    sys_log_debug("[SOUND_TX] seq=%u  len=%u  rl=%d  flags=0x%02X\n",
                  (unsigned)(s_seq - 1u),
                  (unsigned)frameLen,
                  (int)bus.runLevel,
                  (unsigned)( (bus.batteryIsLow ? COMBUS_FLAG_BATTERY_LOW : 0u)
                             | (bus.keyOn        ? COMBUS_FLAG_KEY_ON      : 0u)
                             | (failSafe         ? COMBUS_FLAG_FAILSAFE    : 0u) ));
#endif
}

// EOF sound_uart_tx.cpp
