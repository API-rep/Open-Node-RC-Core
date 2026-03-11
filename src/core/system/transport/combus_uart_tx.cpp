/******************************************************************************
 * @file combus_uart_tx.cpp
 * Generic ComBus UART transmitter — implementation.
 *****************************************************************************/

#include "combus_uart_tx.h"

#include <core/combus/combus_frame.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

/// Active serial port (set at init).
static HardwareSerial* s_serial     = nullptr;

/// Frame header fields fixed at init time.
static uint8_t         s_envId      = 0u;
static uint8_t         s_nAnalog    = 0u;
static uint8_t         s_nDigital   = 0u;

/// Rolling sequence counter (0–255, wraps).
static uint8_t         s_seq        = 0u;

/// Timestamp of last transmitted frame (ms).
static uint32_t        s_lastTxMs   = 0u;

/// Transmit period derived from txHz at init.  Zero = not initialized.
static uint32_t        s_txPeriodMs = 0u;


// =============================================================================
// 2. INITIALIZATION
// =============================================================================

void combus_uart_tx_init( HardwareSerial* serial,
                           uint8_t         envId,
                           uint8_t         nAnalog,
                           uint8_t         nDigital,
                           uint32_t        baud,
                           int             txPin,
                           int             rxPin,
                           uint32_t        txHz ) {

    if (!serial || txHz == 0u) { return; }

    s_serial     = serial;
    s_envId      = envId;
    s_nAnalog    = nAnalog;
    s_nDigital   = nDigital;
    s_txPeriodMs = 1000u / txHz;

    serial->begin(baud, SERIAL_8N1, rxPin, txPin);

    sys_log_info("[UART_TX] init — baud=%u  tx=%d  rx=%d  rate=%uHz  A%u+D%u\n",
                 baud, txPin, rxPin, txHz,
                 (unsigned)nAnalog, (unsigned)nDigital);
}


// =============================================================================
// 3. TRANSMIT UPDATE
// =============================================================================

void combus_uart_tx_update(const ComBus* bus, bool failSafe) {

      // --- Guard: init not done or invalid bus ---
    if (!s_serial || s_txPeriodMs == 0u || !bus) { return; }

      // --- Timer gate ---
    uint32_t now = millis();
    if ((now - s_lastTxMs) < s_txPeriodMs) { return; }
    s_lastTxMs = now;

      // --- Encode ---
    static uint8_t frame[255u];
    uint8_t frameLen = combus_frame_encode(
        frame,
        bus,
        s_nAnalog,
        s_nDigital,
        s_envId,
        s_seq,
        failSafe
    );

    if (frameLen == 0u) { return; }

      // --- Send ---
    s_serial->write(frame, frameLen);
    s_seq++;

    output_log_dbg("[UART_TX] seq=%u  len=%u  rl=%d  flags=0x%02X\n",
                   (unsigned)(s_seq - 1u),
                   (unsigned)frameLen,
                   (int)bus->runLevel,
                   (unsigned)(failSafe ? COMBUS_FLAG_FAILSAFE : 0u));
}

// EOF combus_uart_tx.cpp
