/******************************************************************************
 * @file combus_uart_init.h
 * @brief ComBus UART transport initialisation — shared entry point.
 *
 * @details Centralises UART port resolution and ComBus protocol init for any
 * combination of TX-only, RX-only, or full-duplex direction, as selected by
 * the build-time flags:
 *
 *   -D COMBUS_UART=N     — full-duplex (TX + RX) on UARTn
 *   -D COMBUS_UART_TX=N  — TX-only on UARTn
 *   -D COMBUS_UART_RX=N  — RX-only on UARTn
 *
 * The UART port (uart_com_init) is opened exactly once regardless of direction.
 * TX and RX protocol layers (combus_tx_init / combus_rx_init) are initialised
 * conditionally based on the active flag.
 *
 * The caller retains ownership of analogBuf and digitalBuf — they must have
 * static storage duration because the RX module holds a live pointer to them.
 * Pass nullptr for the inactive direction's buffers.
 *
 * Pin resolution uses the board-level uartPins[] table (extern, defined in
 * the active env's board .cpp — resolved at link time).
 *
 * Typical TX-only caller (machine env):
 * @code
 *   static_assert(ComBusUartBaud <= UartMaxBaud, "baud ceiling");
 *   constexpr ComBusFrameCfg txCfg = { MACHINE_TYPE, N_ANALOG, N_DIGITAL };
 *   combus_uart_init(ComBusUartBaud, txCfg, ComBusUartTxHz, {}, nullptr, nullptr);
 * @endcode
 *
 * Typical RX-only caller (sound env):
 * @code
 *   static uint16_t analog[N_ANALOG];
 *   static bool     digital[N_DIGITAL];
 *   constexpr ComBusFrameCfg rxCfg = { ENV_ID, N_ANALOG, N_DIGITAL };
 *   combus_uart_init(SOUND_UART_BAUD, {}, 0, rxCfg, analog, digital, &pinReg);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>

#include <core/system/hw/pin_reg.h>
#include <struct/outputs_struct.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)

/**
 * @brief Initialise the ComBus UART transport (port + protocol layers).
 *
 * @details Resolves the UART channel and GPIO pins from the active build flag
 * and the board uartPins[] table, opens the UART port once, then initialises
 * the TX and/or RX protocol layers depending on which flag is active.
 *
 * TX side parameters are ignored when only COMBUS_UART_RX is defined.
 * RX side parameters are ignored when only COMBUS_UART_TX is defined.
 *
 * @param baud       UART baud rate.
 * @param txCfg      ComBus frame config for TX (envId, nAnalog, nDigital).
 * @param txHz       TX frame rate in Hz.
 * @param rxCfg      ComBus frame config for RX (envId, nAnalog, nDigital).
 * @param analogBuf  Caller-owned backing buffer for analog channels (RX side).
 * @param digitalBuf Caller-owned backing buffer for digital channels (RX side).
 * @param reg        Optional pin registry — TX and RX pins are claimed if non-null.
 */
void combus_uart_init( uint32_t        baud,
                       ComBusFrameCfg  txCfg,
                       uint32_t        txHz,
                       ComBusFrameCfg  rxCfg,
                       uint16_t*       analogBuf,
                       bool*           digitalBuf,
                       PinReg*         reg = nullptr );

#endif  // COMBUS_UART_TX / COMBUS_UART_RX / COMBUS_UART

// EOF combus_uart_init.h
