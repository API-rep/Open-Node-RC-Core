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

struct UartTxCtx {
    HardwareSerial* port     = nullptr;  ///< active serial port
    uint8_t         envId    = 0u;       ///< env ID embedded in every frame header
    uint8_t         nAnalog  = 0u;       ///< analog channel count per frame
    uint8_t         nDigital = 0u;       ///< digital channel count per frame
    uint8_t         seq      = 0u;       ///< rolling frame sequence counter (0–255)
    uint32_t        lastTxMs = 0u;       ///< timestamp of last transmitted frame (ms)
    uint32_t        periodMs = 0u;       ///< transmit period derived from txHz (0 = uninit)
};

static UartTxCtx s_tx;


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

    s_tx.port     = serial;
    s_tx.envId    = envId;
    s_tx.nAnalog  = nAnalog;
    s_tx.nDigital = nDigital;
    s_tx.periodMs = 1000u / txHz;

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
    if (!s_tx.port || s_tx.periodMs == 0u || !bus) { return; }

      // --- Timer gate ---
    uint32_t now = millis();
    if ((now - s_tx.lastTxMs) < s_tx.periodMs) { return; }
    s_tx.lastTxMs = now;

      // --- Encode ---
    static uint8_t frame[255u];
    uint8_t frameLen = combus_frame_encode(
        frame,
        bus,
        s_tx.nAnalog,
        s_tx.nDigital,
        s_tx.envId,
        s_tx.seq,
        failSafe
    );

    if (frameLen == 0u) { return; }

      // --- Send ---
    s_tx.port->write(frame, frameLen);
    s_tx.seq++;

    output_log_dbg("[UART_TX] seq=%u  len=%u  rl=%d  flags=0x%02X\n",
                   (unsigned)(s_tx.seq - 1u),
                   (unsigned)frameLen,
                   (int)bus->runLevel,
                   (unsigned)(failSafe ? COMBUS_FLAG_FAILSAFE : 0u));
}

// EOF combus_uart_tx.cpp
