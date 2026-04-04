/******************************************************************************
 * @file combus_uart.h
 * @brief ComBus UART protocol layer — wire TX/RX protocol layers to transport.
 *
 * @details Initialises the TX and/or RX ComBus protocol layers over the UART
 * transport that was already opened by uart_init().  Called after hw_init(),
 * from the env-level com_init() wrapper.
 *
 * The UART port and its NodeCom* must have been opened beforehand via
 * uart_init() — this function only wires the protocol layers to the existing
 * transport handle.
 *
 * Build-flag behaviour:
 *   -D COMBUS_UART=N     — full-duplex: TX and RX layers both initialised
 *   -D COMBUS_UART_TX=N  — TX layer only
 *   -D COMBUS_UART_RX=N  — RX layer only
 *
 * The caller retains ownership of analogBuf and digitalBuf — they must have
 * static storage duration because the RX module holds a live pointer to them.
 * Pass nullptr for the inactive direction's buffers.
 *
 * Env-level callers invoke the 0-param combus_uart_init() defined in
 * init/com/combus_uart_init.h, which fills env-specific config and buffers
 * then delegates here.
 *
 * Typical TX-only caller (env wrapper):
 * @code
 *   constexpr ComBusFrameCfg txCfg = { MACHINE_TYPE, N_ANALOG, N_DIGITAL };
 *   combus_protocol_init(txCfg, ComBusUartTxHz, {}, nullptr, nullptr);
 * @endcode
 *
 * Typical RX-only caller (env wrapper):
 * @code
 *   static uint16_t analog[N_ANALOG];
 *   static bool     digital[N_DIGITAL];
 *   constexpr ComBusFrameCfg rxCfg = { ENV_ID, N_ANALOG, N_DIGITAL };
 *   combus_protocol_init({}, 0, rxCfg, analog, digital);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <struct/outputs_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

/**
 * @brief Initialise the ComBus protocol layers over any open transport.
 *
 * @details Passes @p com to combus_tx_init() and/or combus_rx_init()
 * depending on which build flags are active.  The transport itself is
 * opened by the caller (e.g. uart_com_init()) before calling this function.
 *
 * TX side parameters are ignored when only COMBUS_UART_RX is defined.
 * RX side parameters are ignored when only COMBUS_UART_TX is defined.
 *
 * @param com        Open NodeCom* handle (from any *_com_init).
 * @param txCfg      ComBus frame config for TX (envId, nAnalog, nDigital).
 * @param txHz       TX frame rate in Hz.
 * @param rxCfg      ComBus frame config for RX (envId, nAnalog, nDigital).
 * @param analogBuf  Caller-owned backing buffer for analog channels (RX side).
 * @param digitalBuf Caller-owned backing buffer for digital channels (RX side).
 */
void combus_protocol_init( NodeCom*        com,
                           ComBusFrameCfg  txCfg,
                           uint32_t        txHz,
                           ComBusFrameCfg  rxCfg,
                           uint16_t*       analogBuf,
                           bool*           digitalBuf );

#endif  // COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART

// EOF combus_protocol.h
