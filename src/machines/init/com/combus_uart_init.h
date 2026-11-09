/******************************************************************************
 * @file  combus_uart_init.h
 * @brief ComBus link of the board: from the build flag to a running link.
 *
 * @details Active with one of the ComBus UART flags:
 *     -D COMBUS_UART_TX=N  transmit only, on UART N
 *     -D COMBUS_UART=N     both ways, on UART N
 *   An inline no-op without them.
 *
 *   The link itself is started by combus_links_init() (combus_links.h).
 *****************************************************************************/
#pragma once



// =============================================================================
// 1. API
// =============================================================================

#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)

	/// Start the ComBus link chosen by the build flag, on its UART port.
void combus_uart_init();

#else

inline void combus_uart_init() {}

#endif  // COMBUS_UART_TX / COMBUS_UART

// EOF combus_uart_init.h
