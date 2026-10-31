/******************************************************************************
 * @file combus_links.cpp
 * @brief ComBus links from their description — implementation.
 *****************************************************************************/

#include "combus_links.h"

#include <Arduino.h>  // millis()

#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>
#include <core/system/combus/combus_access.h>   // combus_set_raw_analog(), combus_set_digital()
#include <core/system/combus/combus_res.h>      // CbusNeutral

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


    // Chantier 2.3 — the RX buffers must hold the frames of any link: a buffer
    // smaller than the SYSTEM next set does not compile.
static_assert(sizeof(ComLinkRuntime::analogRx) / sizeof(uint16_t)
                  >= static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT)
              && sizeof(ComLinkRuntime::digitalRx) / sizeof(bool)
                  >= static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT),
              "ComLinkRuntime RX buffers smaller than the SYSTEM next set");
static_assert(static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT)  >= static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT)
              && static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT) >= static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT),
              "the SYSTEM set must contain the BASE set");


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

    if (dev.activation == LinkActivation::ON_COUPLING && !dev.coupling) {
        sys_log_err("[COMBUS_LINKS] link '%s': a link ON_COUPLING needs its CouplingDev\n", dev.name);
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

        // --- 2. Frame layout of the scope's view, or of the link through a coupler (3.1b, 3.4a) ---
    const ComPeerLink* peer = map.dev->peer;
    uint8_t nAnalog  = 0u;
    uint8_t nDigital = 0u;
    wireCounts(map.dev->scope, nAnalog, nDigital);
    if (peer) {
        if (map.dev->scope != LinkScope::INTER_NODE) {
            sys_log_err("[COMBUS_LINKS] link '%s': a coupler is for a link between nodes only\n", map.dev->name);
            return false;
        }
        if (peer->nAnalog > sizeof(runtime.analogRx) / sizeof(runtime.analogRx[0])
            || peer->nDigital > sizeof(runtime.digitalRx) / sizeof(runtime.digitalRx[0])) {
            sys_log_err("[COMBUS_LINKS] link '%s': the link through '%s' exceeds the RX buffers\n",
                        map.dev->name, peer->coupler);
            return false;
        }
        nAnalog  = peer->nAnalog;
        nDigital = peer->nDigital;
    }
    const ComBusFrameCfg frameCfg = { nAnalog, nDigital };

        // --- 3. Fill the descriptor ---
    ComBusLink built{};
    built.com          = com;
    built.layer        = linkScopeLayer(map.dev->scope);
    built.handshakeCtx = &runtime.handshake;
    built.name         = map.dev->name;
    built.peer         = peer;

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
    runtime.name           = map.dev->name;
    runtime.receives       = endReceives(map.end);
    runtime.linkTimeoutMs  = map.dev->protocol->linkTimeoutMs;
    runtime.startupGraceMs = map.dev->protocol->startupGraceMs;
    runtime.scope          = map.dev->scope;
    runtime.peer           = peer;
    runtime.coupling       = (map.dev->activation == LinkActivation::ON_COUPLING) ? map.dev->coupling : nullptr;

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
static ComLinkRuntime* s_watched      = nullptr;
static uint8_t         s_watchedCount = 0u;


void combus_links_watch( ComLinkRuntime* runtime, uint8_t count )
{
    s_watched      = runtime;
    s_watchedCount = count;

    const uint32_t now = millis();
    for (uint8_t i = 0u; i < count; ++i) {
            // The startup grace starts now, nothing is reported lost yet
        runtime[i].watchStartMs = now;
        runtime[i].reportedLost = false;
        runtime[i].wasAlive     = false;
        runtime[i].coupled      = false;

            // 3.3: a coupled link starts suspended, until its trigger says hitched
        if (runtime[i].coupling) {
            combus_tx_set_active(i, false);
            combus_rx_set_active(i, false);
        }

            // The RX module needs the same timeouts to ignore silent links in the both_or merge
        if (runtime[i].receives) { combus_rx_set_link_timeout(i, runtime[i].linkTimeoutMs); }
    }
}


ComLinkState combus_link_state( uint8_t linkIdx )
{
    if (linkIdx >= s_watchedCount || !s_watched[linkIdx].receives) { return ComLinkState::NEVER_SEEN; }

    if (combus_rx_age_ms(linkIdx) == UINT32_MAX)                       { return ComLinkState::NEVER_SEEN; }
    if (combus_rx_is_alive(linkIdx, s_watched[linkIdx].linkTimeoutMs)) { return ComLinkState::ALIVE; }
    return ComLinkState::LOST;
}


    // Lost, or never seen once the startup grace has elapsed — the reading of a link between boards.
static bool lostAfterGrace( uint8_t linkIdx )
{
    switch (combus_link_state(linkIdx)) {
        case ComLinkState::LOST:
            return true;
        case ComLinkState::NEVER_SEEN:
            return (millis() - s_watched[linkIdx].watchStartMs) >= s_watched[linkIdx].startupGraceMs;
        case ComLinkState::ALIVE:
        default:
            return false;
    }
}


bool combus_links_lost( LinkScope scope )
{
    for (uint8_t i = 0u; i < s_watchedCount; ++i) {
        const ComLinkRuntime& rt = s_watched[i];
        if (!rt.receives || rt.scope != scope || rt.coupling) { continue; }   // 3.3: a coupled link has its own contributor

            // Link rework S2: a link to another node counts at once (it may not be
            // there); a link between boards waits for the startup grace.
        if (scope == LinkScope::INTER_NODE && combus_link_state(i) != ComLinkState::ALIVE) { return true; }
        if (scope == LinkScope::INTRA_NODE && lostAfterGrace(i))                         { return true; }
    }
    return false;
}


    // Link rework 3.2c — the positions received on a lost link with a peer take
    // back their safe value.  both_or positions are skipped: never written by a
    // frame, the merge ignores a link that is not alive.
static void takeBackSafeValues( const ComPeerLink& peer, ComBus& bus )
{
    for (uint8_t p = 0u; p < peer.nAnalog; ++p) {
        if (!peer.analogRx[p]) { continue; }
        combus_set_raw_analog(bus, static_cast<AnalogComBusID>(peer.analog[p]), CbusNeutral, ChanLayer::BASE);
    }
    for (uint8_t p = 0u; p < peer.nDigital; ++p) {
        if (!peer.digitalRx[p] || bus.digitalBus[peer.digital[p]].bothOr.enabled) { continue; }
        combus_set_digital(bus, static_cast<DigitalComBusID>(peer.digital[p]), false, ChanLayer::BASE);
    }
}


    // Link rework 3.3 — follow the trigger of a coupled link: suspend it when
    // unhitched (received positions back to their safe value), activate it
    // with a new grace when hitched.
static void followCoupling( uint8_t i, ComBus& bus )
{
    ComLinkRuntime& rt = s_watched[i];
    const bool hitched = bus.digitalBus[static_cast<uint8_t>(rt.coupling->trigger)].value;
    if (hitched == rt.coupled) { return; }
    rt.coupled = hitched;

    if (hitched) {
        rt.watchStartMs = millis();
        rt.reportedLost = false;
        combus_rx_set_active(i, true);
        combus_tx_set_active(i, true);
        sys_log_info("[COMBUS_LINKS] link '%s' coupled (%s)\n", rt.name ? rt.name : "?", rt.coupling->name);
        return;
    }

    if (rt.wasAlive && rt.peer) { takeBackSafeValues(*rt.peer, bus); }
    rt.wasAlive     = false;
    rt.reportedLost = false;
    combus_tx_set_active(i, false);
    combus_rx_set_active(i, false);
    sys_log_info("[COMBUS_LINKS] link '%s' uncoupled (%s)\n", rt.name ? rt.name : "?", rt.coupling->name);
}


void combus_links_update( ComBus& bus )
{
    for (uint8_t i = 0u; i < s_watchedCount; ++i) {
        ComLinkRuntime& rt = s_watched[i];
        if (rt.coupling) {
            followCoupling(i, bus);
            if (rt.coupling->fail) {
                combus_set_digital(bus, *rt.coupling->fail, rt.coupled && rt.receives && lostAfterGrace(i), ChanLayer::BOARD);
            }
            if (!rt.coupled) { continue; }
        }
        if (!rt.receives) { continue; }

            // --- 3.2c: a link with a peer that stops being alive: safe values ---
        const bool alive = (combus_link_state(i) == ComLinkState::ALIVE);
        if (rt.wasAlive && !alive && rt.peer) {
            takeBackSafeValues(*rt.peer, bus);
            sys_log_warn("[COMBUS_LINKS] link '%s': received channels back to their safe value\n",
                         rt.name ? rt.name : "?");
        }
        rt.wasAlive = alive;

        const bool lost = lostAfterGrace(i);
        if (lost == rt.reportedLost) { continue; }
        rt.reportedLost = lost;

        if (lost) {
            sys_log_warn("[COMBUS_LINKS] link '%s' lost (%s)\n", rt.name ? rt.name : "?",
                         rt.scope == LinkScope::INTRA_NODE ? "between boards" : "to another node");
        } else {
            sys_log_info("[COMBUS_LINKS] link '%s' back\n", rt.name ? rt.name : "?");
        }
    }
}


bool combus_links_rx_lost()
{
    for (uint8_t i = 0u; i < s_watchedCount; ++i) {
        if (s_watched[i].coupling && !s_watched[i].coupled) { continue; }   // 3.3: unhitched, not lost
        if (s_watched[i].receives && !combus_rx_is_alive(i, s_watched[i].linkTimeoutMs)) {
            return true;
        }
    }
    return false;
}

// EOF combus_links.cpp
