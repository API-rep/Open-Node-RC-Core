/******************************************************************************
 * @file combus_uart_init.cpp
 * @brief ComBus UART transport env init — machine env implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

#include "combus_uart_init.h"

#include <machines/config/config.h>
#include <core/config/outputs/combus_uart.h>         // ComBusUartBaud, ComBusUartTxHz, UartMaxBaud
#include <core/system/combus/protocol/combus_uart.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. COMBUS UART INIT
// =============================================================================

void combus_uart_init()
{
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

    constexpr ComBusFrameCfg txCfg = {
        static_cast<uint8_t>(CombusLayout::MACHINE_TYPE),
        static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
        static_cast<uint8_t>(DigitalComBusID::CH_COUNT),
    };

    // --- Full-duplex: also initialise RX side ---
    #if defined(COMBUS_UART)
      static uint16_t s_analog[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)];
      static bool     s_digital[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)];
      constexpr ComBusFrameCfg rxCfg = {
          static_cast<uint8_t>(CombusLayout::MACHINE_TYPE),
          static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
          static_cast<uint8_t>(DigitalComBusID::CH_COUNT),
      };
      combus_protocol_init(txCfg, ComBusUartTxHz, rxCfg, s_analog, s_digital);
    #else
      combus_protocol_init(txCfg, ComBusUartTxHz, {}, nullptr, nullptr);
    #endif
}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.cpp
