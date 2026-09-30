/******************************************************************************
 * @file combus_uart_init.cpp
 * @brief ComBus UART transport env init — machine env implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

#include "combus_uart_init.h"

#include <machines/config/config.h>   // transitively includes <combus_remote_ids.h> via dumper_truck/combus_ids_remote.h
#include <core/config/outputs/combus_uart.h>         // ComBusUartBaud, ComBusUartTxHz, UartMaxBaud
#include <core/system/combus/protocol/combus_protocol.h>
#include <core/system/combus/protocol/combus_tx.h>    // CombusTxState, combus_tx_register_pool
#include <core/system/combus/protocol/combus_rx.h>    // CombusRxState, combus_rx_register_pool
#include <core/system/combus/protocol/frame/combus_handshake.h>  // CombusHandshakeContext (P3)
#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>
#include "../sys/sys_init.h"          // pinReg (machine env)


// =============================================================================
// 1. COMBUS UART INIT
// =============================================================================

// Phase 4 (A14) + Phase 5 (A15): wire dimensions are derived from the
// Remote view's ID enums directly. Since A15, each TYPE has its own
// 0-based counter (analog and digital are independently numbered), so
// CH_COUNT for each enum equals exactly the count of channels of that
// type in the Remote view — no subtraction needed.
static constexpr uint8_t kRemoteAnalogCount =
    static_cast<uint8_t>(AnalogComBusRemoteID::CH_COUNT);
static constexpr uint8_t kRemoteDigitalCount =
    static_cast<uint8_t>(DigitalComBusRemoteID::CH_COUNT);

void combus_uart_init()
{
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

        // P3 — caller-owned per-link handshake context.  Zero-initialised
        //    at boot, shared between TX and RX of the same ComBus link
        //    by combus_protocol_init().  Multiple independent ComBus
        //    interfaces must each declare their own context.
    static CombusHandshakeContext s_linkHandshakeCtx = {};

    constexpr ComBusFrameCfg txCfg = {
        kRemoteAnalogCount,                               ///< Analog wire channels only — not local sound-node channels.
        kRemoteDigitalCount,                              ///< Digital wire channels only — not local sound-node channels.
    };


    // --- Resolve UART channel and GPIO pins from build flag (was in uart_init()) ---
    #if defined(COMBUS_UART)
        constexpr int uartCh    = COMBUS_UART;
        const     int uartTxPin = uartPins[uartCh].tx;
        const     int uartRxPin = uartPins[uartCh].rx;
    #elif defined(COMBUS_UART_TX)
        constexpr int uartCh    = COMBUS_UART_TX;
        const     int uartTxPin = uartPins[uartCh].tx;
        constexpr int uartRxPin = -1;
    #endif

    // --- Open the ComBus UART port directly (no single-link helper in uart_com.h) ---
    NodeCom* com = uart_com_init(uart_serial_for(uartCh), ComBusUartBaud,
                                 uartTxPin, uartRxPin, "combus", &pinReg);

    // --- Build the ComBusLink descriptor (single source of truth) ---
    ComBusLink link{};
    link.com          = com;
    link.layer        = ChanLayer::REMOTE;   ///< See audit note in commit message — REMOTE matches the
                                             ///< "Remote" suffix in AnalogComBusRemoteID / kRemoteAnalogCount.
                                             ///< LY2 — also selects the wire-end counters at TX init
                                             ///< (REMOTE → CH_COUNT of the Remote view).
    link.handshakeCtx = &s_linkHandshakeCtx;
    link.name         = "combus";

    // --- TX side: active when COMBUS_UART_TX or full-duplex COMBUS_UART ---
    #if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
      link.txCfg = txCfg;
      link.txHz  = ComBusUartTxHz;
    #endif

    // --- RX side: active only when full-duplex COMBUS_UART ---
    #if defined(COMBUS_UART)
      static uint16_t s_analog[kRemoteAnalogCount];
      static bool     s_digital[kRemoteDigitalCount];
      constexpr ComBusFrameCfg rxCfg = {
          kRemoteAnalogCount,                             ///< Analog wire channels only.
          kRemoteDigitalCount,                            ///< Digital wire channels only.
      };
      link.rxCfg      = rxCfg;
      link.analogBuf  = s_analog;
      link.digitalBuf = s_digital;
    #endif

    // --- Caller-owned link array — its size IS the link count ---
    ComBusLink links[] = { link };

    // --- Caller-owned TX state pool — its size IS the link count ---
    static CombusTxState txStates[sizeof(links) / sizeof(links[0])];
    combus_tx_register_pool(txStates, sizeof(links) / sizeof(links[0]));

    // --- Caller-owned RX state pool — its size IS the link count ---
    static CombusRxState rxStates[sizeof(links) / sizeof(links[0])];
    combus_rx_register_pool(rxStates, sizeof(links) / sizeof(links[0]));

    combus_protocol_init_all(links, sizeof(links) / sizeof(links[0]));
}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.cpp