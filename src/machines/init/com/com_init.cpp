/******************************************************************************
 * @file com_init.cpp
 * @brief Machine env communication transports initialisation — orchestrator.
 *
 * @details Dispatches to each active transport sub-init based on compile flags.
 * Each sub-init is self-contained and degrades to a no-op when its flag is absent.
 *
 *   Active transports:
 *     COMBUS_UART_TX=N, COMBUS_UART_RX=N, COMBUS_UART=N — ComBus on UARTn (combus_uart_init.h)
 *
 *   Future:
 *     (add new transport sub-inits here)
 *****************************************************************************/

#include "com_init.h"
#include "combus_uart_init.h"


// =============================================================================
// 1. COM INIT ORCHESTRATOR
// =============================================================================

void com_init()
{
    combus_uart_init();  // no-op if no ComBus UART flag is defined
}

// EOF com_init.cpp
