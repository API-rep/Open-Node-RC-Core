/******************************************************************************
 * @file combus_links.cpp
 * @brief ComBus links from their description — implementation.
 *****************************************************************************/

#include "combus_links.h"

#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>

// The 3 view-specific ID headers give the channel counts of each scope.
// combus.h transitively includes combus_ids.h (FULL view).
#include "combus.h"
#include "combus_system_ids.h"
#include "combus_base_ids.h"


// =============================================================================
// 1. HELPERS
// =============================================================================

    // Channel counts of the view carried by a scope — the same counters the TX
    // resolves from the layer (see combus_tx_init()).
static void wireCounts( LinkScope scope, uint8_t& nAnalog, uint8_t& nDigital )
{
    if (scope == LinkScope::INTER_NODE) {
        nAnalog  = static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT);
        nDigital = static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT);
    } else {
        nAnalog  = static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT);
        nDigital = static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT);
    }
}


static bool endTransmits( LinkEnd end ) { return end != LinkEnd::RX; }
static bool endReceives ( LinkEnd end ) { return end != LinkEnd::TX; }


    // Open the port of a wired link and return its transport (nullptr on failure).
    // An end that does not transmit (or receive) leaves its pin unclaimed.
static NodeCom* openPort( const ComDevMap& map, PinReg* pinReg )
{
    const ComPort& port = *map.port;

    switch (port.type) {
        case ComPortType::UART: {
            const int txPin = endTransmits(map.end) ? port.uart.txPin : -1;
            const int rxPin = endReceives (map.end) ? port.uart.rxPin : -1;
            return uart_com_init(uart_serial_for(port.uart.index), map.dev->protocol->rate,
                                 txPin, rxPin, map.dev->name, pinReg);
        }
        case ComPortType::NONE:
        default:
            sys_log_err("[COMBUS_LINKS] link '%s': port '%s' has no usable type\n",
                        map.dev->name, port.name);
            return nullptr;
    }
}


// =============================================================================
// 2. CHECK AND BUILD
// =============================================================================

bool combus_link_check( const ComDevMap& map )
{
    if (!map.dev || !map.port || !map.dev->protocol) {
        sys_log_err("[COMBUS_LINKS] wiring rejected — null device, port or protocol\n");
        return false;
    }
    const ComDev& dev = *map.dev;

    if (dev.activation != LinkActivation::ALWAYS) {
        sys_log_err("[COMBUS_LINKS] link '%s': activation other than ALWAYS is not supported yet\n", dev.name);
        return false;
    }
    if (dev.type == LinkType::BIDIRECTIONAL && map.end != LinkEnd::TX_RX) {
        sys_log_err("[COMBUS_LINKS] link '%s': a bidirectional link must be wired TX_RX\n", dev.name);
        return false;
    }
    if (dev.type == LinkType::UNIDIRECTIONAL && map.end == LinkEnd::TX_RX) {
        sys_log_err("[COMBUS_LINKS] link '%s': a unidirectional link cannot be wired TX_RX\n", dev.name);
        return false;
    }
    if (endTransmits(map.end) && dev.protocol->txHz == 0u) {
        sys_log_err("[COMBUS_LINKS] link '%s': a transmitting end needs a non-zero txHz\n", dev.name);
        return false;
    }
    if (endReceives(map.end) && dev.protocol->linkTimeoutMs == 0u) {
        sys_log_err("[COMBUS_LINKS] link '%s': a receiving end needs a non-zero linkTimeoutMs\n", dev.name);
        return false;
    }
    if (map.port->type == ComPortType::UART && dev.protocol->rate > map.port->uart.maxBaud) {
        sys_log_err("[COMBUS_LINKS] link '%s': rate %lu exceeds the ceiling of port '%s' (%lu)\n",
                    dev.name, (unsigned long)dev.protocol->rate,
                    map.port->name, (unsigned long)map.port->uart.maxBaud);
        return false;
    }
    return true;
}


bool combus_link_build( const ComDevMap& map,
                        NodeCom*         com,
                        ComLinkRuntime&  runtime,
                        ComBusLink&      link )
{
        // --- 1. Validate ---
    if (!com || !combus_link_check(map)) { return false; }

        // --- 2. Frame layout of the scope's view ---
    uint8_t nAnalog  = 0u;
    uint8_t nDigital = 0u;
    wireCounts(map.dev->scope, nAnalog, nDigital);
    const ComBusFrameCfg frameCfg = { nAnalog, nDigital };

        // --- 3. Fill the descriptor ---
    ComBusLink built{};
    built.com          = com;
    built.layer        = linkScopeLayer(map.dev->scope);
    built.handshakeCtx = &runtime.handshake;
    built.name         = map.dev->name;

    if (endTransmits(map.end)) {
        built.txCfg = frameCfg;
        built.txHz  = map.dev->protocol->txHz;
    }
    if (endReceives(map.end)) {
        built.rxCfg      = frameCfg;
        built.analogBuf  = runtime.analogRx;
        built.digitalBuf = runtime.digitalRx;
    }

        // --- 4. What the link watch needs ---
    runtime.receives      = endReceives(map.end);
    runtime.linkTimeoutMs = map.dev->protocol->linkTimeoutMs;

    link = built;
    return true;
}


// =============================================================================
// 3. INIT
// =============================================================================

bool combus_links_init( const ComDevMap*       maps,
                        uint8_t                count,
                        const ComLinksStorage& storage,
                        PinReg*                pinReg,
                        ComBus*                target )
{
    if (!maps || count == 0u || !storage.tx || !storage.rx || !storage.runtime || !storage.links) {
        sys_log_err("[COMBUS_LINKS] init rejected — null table or storage\n");
        return false;
    }

        // --- 1. Open each port and build its link (a failure leaves the slot empty) ---
    bool allUp = true;
    for (uint8_t i = 0u; i < count; ++i) {
        storage.links[i] = ComBusLink{};

        if (!combus_link_check(maps[i])) { allUp = false; continue; }

        NodeCom* com = openPort(maps[i], pinReg);
        if (!combus_link_build(maps[i], com, storage.runtime[i], storage.links[i])) {
            allUp = false;
            continue;
        }

            // Received frames are applied to the target ComBus, on the receiving links only
        if (storage.links[i].rxCfg.has_value()) { storage.links[i].target = target; }
    }

        // --- 2. Pools, then the protocol layers ---
    combus_tx_register_pool(storage.tx, count);
    combus_rx_register_pool(storage.rx, count);
    combus_protocol_init_all(storage.links, count);

        // --- 3. Watch the receiving links ---
    combus_links_watch(storage.runtime, count);

    return allUp;
}


// =============================================================================
// 4. LINK WATCH
// =============================================================================

    // Per-link storage under watch — caller-owned, static (see combus_links_watch()).
static const ComLinkRuntime* s_watched      = nullptr;
static uint8_t               s_watchedCount = 0u;


void combus_links_watch( const ComLinkRuntime* runtime, uint8_t count )
{
    s_watched      = runtime;
    s_watchedCount = count;
}


bool combus_links_rx_lost()
{
    for (uint8_t i = 0u; i < s_watchedCount; ++i) {
        if (s_watched[i].receives && !combus_rx_is_alive(i, s_watched[i].linkTimeoutMs)) {
            return true;
        }
    }
    return false;
}

// EOF combus_links.cpp
