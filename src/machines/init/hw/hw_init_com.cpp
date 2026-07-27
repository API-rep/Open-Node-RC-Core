/******************************************************************************
 * @file hw_init_com.cpp
 * @brief Communication transport hardware initialisation — implementation.
 *****************************************************************************/

#include "hw_init_com.h"

#include <machines/config/config.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>
#include "../sys/sys_init.h"


void hw_init_com()
{
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART) || defined(COMBUS_UART_RX)
      // Static storage for the shared UART port pool — owned by the machine,
      // sized to this board's real capacity. Must outlive the program.
    static UartCtx s_uartPool[UartComMaxPorts];
    uart_com_register_pool(s_uartPool, UartComMaxPorts);

    uart_init(ComBusUartBaud, UartComMaxPorts, &pinReg);
#endif
}

// EOF hw_init_com.cpp
