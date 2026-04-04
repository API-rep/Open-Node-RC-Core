/******************************************************************************
 * @file combus_uart.cpp
 * @brief ComBus UART protocol layer — implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

#include "combus_uart.h"

#include <core/system/hw/transport/uart_com.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. COMBUS PROTOCOL INIT
// =============================================================================

void combus_protocol_init( ComBusFrameCfg  txCfg,
                           uint32_t        txHz,
                           ComBusFrameCfg  rxCfg,
                           uint16_t*       analogBuf,
                           bool*           digitalBuf )
{
    NodeCom* com = uart_get_combus_com();

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

// EOF combus.cpp
