/******************************************************************************
 * @file combus_uart_init.cpp
 * @brief ComBus UART transport initialisation — implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

#include "combus_uart_init.h"

#include <Arduino.h>
#include <core/system/com/ports/uart_com.h>
#include <core/system/com/protocols/combus_tx.h>
#include <core/system/com/protocols/combus_rx.h>
#include <core/system/debug/debug.h>
#include <struct/uart_struct.h>

// Board-level UART pin table — defined in the active env's board .cpp.
// Resolved at link time (volvo_A60H_bruder.cpp / sound_board_esp32.cpp, …).
extern const UartPinCfg uartPins[];


// =============================================================================
// 1. COMBUS UART INIT
// =============================================================================

void combus_uart_init( uint32_t        baud,
                       ComBusFrameCfg  txCfg,
                       uint32_t        txHz,
                       ComBusFrameCfg  rxCfg,
                       uint16_t*       analogBuf,
                       bool*           digitalBuf,
                       PinReg*         reg )
{
    // --- Resolve UART channel and GPIO pins from build flag ---
    #if defined(COMBUS_UART)
      constexpr int uartCh    = COMBUS_UART;           // full-duplex channel
      const     int uartTxPin = uartPins[uartCh].tx;
      const     int uartRxPin = uartPins[uartCh].rx;
    #elif defined(COMBUS_UART_TX)
      constexpr int uartCh    = COMBUS_UART_TX;        // TX-only channel
      const     int uartTxPin = uartPins[uartCh].tx;
      constexpr int uartRxPin = -1;                    // RX not enabled
    #else  // COMBUS_UART_RX
      constexpr int uartCh    = COMBUS_UART_RX;        // RX-only channel
      constexpr int uartTxPin = -1;                    // TX not enabled
      const     int uartRxPin = uartPins[uartCh].rx;
    #endif

    // --- Board-specific RX pin mode (before serial.begin) ---
    #if defined(COMBUS_UART) || defined(COMBUS_UART_RX)
      if (uartRxPin >= 0) {
        pinMode(uartRxPin, INPUT_PULLDOWN);
      }
    #endif

    // --- Open UART port once ---
    NodeCom* com = uart_com_init(uart_serial_for(uartCh), baud, uartTxPin, uartRxPin,
                                 "combus", reg);

    sys_log_info("[COMBUS] UART%d init — tx=%d rx=%d baud=%u.\n",
                 uartCh, uartTxPin, uartRxPin, baud);

    // --- TX protocol layer ---
    #if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
      combus_tx_init(com, txCfg, txHz);
    #endif

    // --- RX protocol layer ---
    #if defined(COMBUS_UART_RX) || defined(COMBUS_UART)
      combus_rx_init(com, rxCfg, analogBuf, digitalBuf);
    #endif
}

#endif  // COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART

// EOF combus_uart_init.cpp
