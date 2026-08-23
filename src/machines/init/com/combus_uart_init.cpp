/******************************************************************************
 * @file combus_uart_init.cpp
 * @brief ComBus UART transport env init — machine env implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

#include "combus_uart_init.h"

#include <machines/config/config.h>
#include <core/config/outputs/combus_uart.h>         // ComBusUartBaud, ComBusUartTxHz, UartMaxBaud
#include <core/system/combus/protocol/combus_protocol.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. COMBUS UART INIT
// =============================================================================

void combus_uart_init()
{
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

    constexpr ComBusFrameCfg txCfg = {
        ComBusWireEndAnalog,                             ///< Analog wire channels only — not local sound-node channels.
        ComBusWireEndDigital,                            ///< Digital wire channels only — not local sound-node channels.
    };


    // --- Full-duplex: also initialise RX side ---
    #if defined(COMBUS_UART)
      static uint16_t s_analog[ComBusWireEndAnalog];
      static bool     s_digital[ComBusWireEndDigital];
      constexpr ComBusFrameCfg rxCfg = {
          ComBusWireEndAnalog,                             ///< Analog wire channels only.
          ComBusWireEndDigital,                            ///< Digital wire channels only.
      };

      combus_protocol_init(uart_get_combus_com(), txCfg, ComBusUartTxHz, rxCfg, s_analog, s_digital);
    #else
      combus_protocol_init(uart_get_combus_com(), txCfg, ComBusUartTxHz, {}, nullptr, nullptr);
    #endif
}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.cpp
