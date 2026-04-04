/******************************************************************************
 * @file combus_uart_init.h
 * @brief ComBus UART transport env init — machine env.
 *
 * @details Handles ComBus UART transport setup for the machine environment.
 *   Active when any ComBus UARTx TX flag is defined:
 *     -D COMBUS_UART_TX=N  — TX-only on UARTn
 *     -D COMBUS_UART=N     — full-duplex (TX + RX) on UARTn
 *   Degrades to an inline no-op when none of these flags are set.
 *
 *   Delegates protocol layer wiring to the core entry point:
 *   combus_protocol_init() in <core/system/com/protocols/combus.h>.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. API
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

/// Initialise the ComBus UART transport (machine env: TX frame config, baud, rate).
void combus_uart_init();

#else

inline void combus_uart_init() {}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.h
