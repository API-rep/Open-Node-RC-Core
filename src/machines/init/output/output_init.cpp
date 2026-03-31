/******************************************************************************
 * @file output_init.cpp
 * @brief Implementation of output peripherals initialization.
 *****************************************************************************/

#include "output_init.h"
#include <machines/config/config.h>
#include <core/system/debug/debug.h>

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  #include <core/system/com/ports/uart_com.h>
  #include <core/system/com/protocols/combus_tx.h>
#endif


// =============================================================================
// 1. OUTPUT INITIALIZATION
// =============================================================================

/**
 * @brief Initialise output transport layer.
 *
 * @details Each output module is conditionally compiled by build flags.
 * Adding a new output peripheral means adding its init call here, guarded by its
 * own build flag. Multiple non conflictual modules can be active at the same time
 * (ex: UART + wireless outputs).
 * Pin assignments come from the active environment configuration (ex: boards/*.h).
 */

void output_init() {
  sys_log_info("[OUTPUT] output_init ...\n");

	// --- ComBus UART TX ---
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  // Board ceiling check — UartMaxBaud in scope (board header resolved via machines/config/config.h).
  static_assert(ComBusUartBaud <= UartMaxBaud,
                "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");
  {
    #if defined(COMBUS_UART)
      constexpr int uartCh    = COMBUS_UART;           // uartChannel (full duplex)
      const     int uartTxPin = uartPins[uartCh].tx;   // TX pin from board UART pin table
      const     int uartRxPin = uartPins[uartCh].rx;   // RX pin from board UART pin table
    #else  // COMBUS_UART_TX
      constexpr int uartCh    = COMBUS_UART_TX;        // uartChannel (TX-only link)
      const     int uartTxPin = uartPins[uartCh].tx;   // TX pin from board UART pin table
      constexpr int uartRxPin = -1;                    // TX-only: RX not enabled
    #endif
    NodeCom* com = uart_com_init(uart_serial_for(uartCh), ComBusUartBaud, uartTxPin, uartRxPin, "sound_tx");
    constexpr ComBusFrameCfg comBusFrameCfg = {
        static_cast<uint8_t>(CombusLayout::MACHINE_TYPE),
        static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
        static_cast<uint8_t>(DigitalComBusID::CH_COUNT),
    };
    combus_tx_init(com, comBusFrameCfg, ComBusUartTxHz);
  }
#endif

  sys_log_info("[OUTPUT] output_init done\n\n");
}

// EOF output_init.cpp
