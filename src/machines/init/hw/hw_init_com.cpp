/******************************************************************************
 * @file hw_init_com.cpp
 * @brief Communication transport hardware initialisation — implementation.
 *****************************************************************************/

#include "hw_init_com.h"

#include <machines/config/config.h>
#include <core/system/com/transport/uart_com.h>
#include <core/system/debug/debug.h>
#include "../sys/sys_init.h"


// =============================================================================
// 1. COM TRANSPORT INIT
// =============================================================================

void hw_init_com()
{
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART) || defined(COMBUS_UART_RX)
    uart_init(ComBusUartBaud, &pinReg);
#endif
}

// EOF hw_init_com.cpp
