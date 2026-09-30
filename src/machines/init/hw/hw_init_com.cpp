/******************************************************************************
 * @file hw_init_com.cpp
 * @brief Communication transport hardware initialisation — implementation.
 *
 * @details Registers the shared UART port pool. The actual ComBus port
 *   opening is now performed by combus_uart_init() (called later in the
 *   boot sequence from com_init()), which calls uart_com_init() directly
 *   with the channel + pins resolved from the active COMBUS_UART* flag.
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
      // combus_uart_init() (called later from com_init()) will claim a slot
      // from this pool via uart_com_init().
    static UartCtx s_uartPool[UartComMaxPorts];
    uart_com_register_pool(s_uartPool, UartComMaxPorts);
#endif
}

// EOF hw_init_com.cpp
