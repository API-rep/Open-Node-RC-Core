/******************************************************************************
 * @file  remote_links.cpp
 * @brief Links of the remote to the machines it drives (chantier 4, embryo).
 *
 * @details The embryo drives one machine: its ComBus is the target of the
 *   link and the source of the frames.  Several machines (step 4.1) need a
 *   target and a source per link (combus_links_init() and combus_tx_update()
 *   take one ComBus today).
 *****************************************************************************/

#include "remote_links.h"

#include <core/system/combus/protocol/combus_links.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/hw/transport/uart_com.h>       // uart_com_register_pool, UartCtx
#include <core/system/debug/logging/debug.h>
#include <remotes/config/driven_machines.h>


    // Caller-owned storage of the links, one slot per driven machine.
static UartCtx        s_uart[kDrivenMachineCount];
static CombusTxState  s_tx[kDrivenMachineCount];
static CombusRxState  s_rx[kDrivenMachineCount];
static ComLinkRuntime s_runtime[kDrivenMachineCount];
static ComBusLink     s_links[kDrivenMachineCount];

static_assert(kDrivenMachineCount == 1u, "the embryo drives one machine: several need a ComBus per link (step 4.1)");


uint8_t remote_links_init( void )
{
    uart_com_register_pool(s_uart, kDrivenMachineCount);

    ComDevMap maps[kDrivenMachineCount];
    for (uint8_t i = 0u; i < kDrivenMachineCount; ++i) { maps[i] = kDrivenMachines[i].link; }

    const ComLinksStorage storage = { s_tx, s_rx, s_runtime, s_links };
    if (!combus_links_init(maps, kDrivenMachineCount, storage, nullptr, kDrivenMachines[0].bus)) {
        sys_log_err("[REMOTE] links not started\n");
        return 0u;
    }
    sys_log_info("[REMOTE] driving %u machine(s): %s\n", (unsigned)kDrivenMachineCount, kDrivenMachines[0].name);
    return kDrivenMachineCount;
}


ComLinkRuntime* remote_links_runtime( uint8_t idx )
{
    return (idx < kDrivenMachineCount) ? &s_runtime[idx] : nullptr;
}


void remote_links_update( void )
{
    ComBus& bus = *kDrivenMachines[0].bus;
    combus_rx_update();
    combus_links_update(bus);
    combus_tx_update(&bus);
}

// EOF remote_links.cpp
