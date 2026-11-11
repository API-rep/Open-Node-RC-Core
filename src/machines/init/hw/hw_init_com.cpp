/******************************************************************************
 * @file hw_init_com.cpp
 * @brief Communication transport hardware initialisation — implementation.
 *
 * @details Registers the shared UART port pool. It is called from sys_init(),
 *   before the first port is claimed (the debug serial): without the pool every
 *   uart_com_init() is refused. The ComBus ports are opened later by
 *   board_links_init() (called from com_init()), from the table of links of the
 *   board (comLinks[]).
 *****************************************************************************/

#include "hw_init_com.h"

#include <machines/config/config.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>
#include "../sys/sys_init.h"


void hw_init_com()
{
      // Static storage for the shared UART port pool — owned by the machine,
      // sized to this board's real capacity. Must outlive the program.
      // Every uart_com_init() claims a slot from this pool: the debug serial
      // (sys_init()) and the ComBus ports (board_links_init()).
    static UartCtx s_uartPool[UartComMaxPorts];
    uart_com_register_pool(s_uartPool, UartComMaxPorts);
}

// EOF hw_init_com.cpp
