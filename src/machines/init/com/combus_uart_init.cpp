/******************************************************************************
 * @file combus_uart_init.cpp
 * @brief ComBus UART transport env init — machine env implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

#include "combus_uart_init.h"

#include <machines/config/config.h>   // transitively includes <combus_remote_ids.h> via dumper_truck/combus_ids_remote.h
#include <core/config/outputs/combus_uart.h>         // ComBusUartBaud, ComBusUartTxHz, UartMaxBaud
#include <core/system/combus/protocol/combus_protocol.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. COMBUS UART INIT
// =============================================================================

// Phase 4 (A14): wire dimensions are now derived from the Remote view's
// ID enums directly. Digital IDs continue after analog IDs in the Remote
// view, so the digital count is the difference between the two CH_COUNT
// sentinels. This replaces the temporary ComBusWireEndAnalog/Digital
// constants emitted by the generator (removed in this same phase).
static constexpr uint8_t kRemoteAnalogCount =
    static_cast<uint8_t>(AnalogComBusRemoteID::CH_COUNT);
static constexpr uint8_t kRemoteDigitalCount =
    static_cast<uint8_t>(DigitalComBusRemoteID::CH_COUNT) - kRemoteAnalogCount;

void combus_uart_init()
{
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

    constexpr ComBusFrameCfg txCfg = {
        kRemoteAnalogCount,                               ///< Analog wire channels only — not local sound-node channels.
        kRemoteDigitalCount,                              ///< Digital wire channels only — not local sound-node channels.
    };


    // --- Full-duplex: also initialise RX side ---
    #if defined(COMBUS_UART)
      static uint16_t s_analog[kRemoteAnalogCount];
      static bool     s_digital[kRemoteDigitalCount];
      constexpr ComBusFrameCfg rxCfg = {
          kRemoteAnalogCount,                             ///< Analog wire channels only.
          kRemoteDigitalCount,                            ///< Digital wire channels only.
      };

      combus_protocol_init(uart_get_combus_com(), txCfg, ComBusUartTxHz, rxCfg, s_analog, s_digital);
    #else
      combus_protocol_init(uart_get_combus_com(), txCfg, ComBusUartTxHz, {}, nullptr, nullptr);
    #endif
}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.cpp
