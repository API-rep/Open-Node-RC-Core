/******************************************************************************
 * @file combus_protocol.cpp
 * @brief ComBus protocol layer — transport-agnostic implementation.
 *****************************************************************************/

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

#include "combus_protocol.h"

#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/frame/combus_handshake.h>  // CombusHandshakeContext
#include <core/system/combus/frame/combus_handshake_rx.h>  // (no-op, ensures include path)



// =============================================================================
// 1. COMBUS PROTOCOL INIT
// =============================================================================


void combus_protocol_init( NodeCom*               com,
                           ComBusFrameCfg         txCfg,
                           uint32_t               txHz,
                           ComBusFrameCfg         rxCfg,
                           uint16_t*              analogBuf,
                           bool*                  digitalBuf,
                           CombusHandshakeContext* handshakeCtx )
{
        // --- Wire the per-link handshake context BEFORE init so the
        //    burst is armed on combus_tx_init() and the contract-
        //    validated flag is shared between TX and RX.  P3.
    combus_tx_set_handshake_ctx(handshakeCtx);
    combus_rx_set_handshake_ctx(handshakeCtx);


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

// EOF combus_protocol.cpp
