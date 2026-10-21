/******************************************************************************
 * @file combus_uart_init.cpp
 * @brief ComBus UART transport env init — machine env implementation.
 *
 * @details Chooses the ComDev and the board port from the COMBUS_UART* build
 *   flag, then hands the wiring to combus_links_init(), which opens the port and
 *   brings the link up. The flags are transitional: they select the wiring
 *   until the board declares its own ComDevMap table.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

#include "combus_uart_init.h"

#include <machines/config/config.h>   // comPorts[], UartMaxBaud (board envCfg)
#include <core/config/outputs/combus_uart.h>         // ComBusUartBaud, kComDevUartUni, kComDevUartBi
#include <core/system/combus/protocol/combus_links.h>
#include <core/system/debug/logging/debug.h>
#include "../sys/sys_init.h"          // pinReg (machine env)


// =============================================================================
// 1. BOARD PORT LOOKUP
// =============================================================================

    // The board port that is UART number `uartIdx`, or nullptr if the board has none.
static const ComPort* findUartPort( uint8_t uartIdx )
{
    for (uint8_t i = 0u; i < comPortsCount; ++i) {
        if (comPorts[i].type == ComPortType::UART && comPorts[i].uart.index == uartIdx) {
            return &comPorts[i];
        }
    }
    return nullptr;
}


// =============================================================================
// 2. COMBUS UART INIT
// =============================================================================

void combus_uart_init()
{
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

        // --- Resolve the ComDev, the UART number and the role of this end from the build flag ---
    #if defined(COMBUS_UART)
        constexpr uint8_t uartCh = COMBUS_UART;
        const ComDev*     dev    = &kComDevUartBi;        // full-duplex
        constexpr LinkEnd end    = LinkEnd::TX_RX;
    #else
        constexpr uint8_t uartCh = COMBUS_UART_TX;
        const ComDev*     dev    = &kComDevUartUni;       // TX only
        constexpr LinkEnd end    = LinkEnd::TX;
    #endif

    const ComPort* port = findUartPort(uartCh);
    if (port == nullptr) {
        sys_log_err("[COMBUS_UART] this board has no UART%u port — ComBus link not started\n", (unsigned)uartCh);
        return;
    }

        // --- Wiring of the link, and the caller-owned storage for it (its size IS the link count) ---
    const ComDevMap maps[] = { { dev, port, end } };
    constexpr uint8_t kLinkCount = 1u;

    static CombusTxState  txStates[kLinkCount];
    static CombusRxState  rxStates[kLinkCount];
    static ComLinkRuntime runtime[kLinkCount];
    static ComBusLink     links[kLinkCount];
    const ComLinksStorage storage = { txStates, rxStates, runtime, links };

    combus_links_init(maps, kLinkCount, storage, &pinReg);
}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.cpp
