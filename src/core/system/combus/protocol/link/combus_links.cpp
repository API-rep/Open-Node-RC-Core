/******************************************************************************
 * @file  combus_links.cpp
 * @brief ComBus links from their description: validate, open the ports, start.
 *
 * @details Nothing is allocated here: every piece of state belongs to the caller.
 *****************************************************************************/

#include "combus_links.h"

#include <string.h>   // memcmp()

#include <core/system/combus/protocol/link/combus_link_watch.h>   // combus_links_watch()
#include <core/system/hw/transport/uart_com.h>
#include <core/system/debug/logging/debug.h>

#include "combus.h"             // AnalogComBusID, DigitalComBusID
#include "combus_system_ids.h"  // size of the SYSTEM view
#include "combus_base_ids.h"    // size of the BASE view



// =============================================================================
// 1. HELPERS
// =============================================================================

	// The RX buffers must hold the frames of any link: smaller ones do not compile.
static_assert(sizeof(ComLinkRuntime::analogRx) / sizeof(uint16_t)
                  >= static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT)
              && sizeof(ComLinkRuntime::digitalRx) / sizeof(bool)
                  >= static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT),
              "ComLinkRuntime RX buffers smaller than the SYSTEM next set");
static_assert(static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT)  >= static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT)
              && static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT) >= static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT),
              "the SYSTEM set must contain the BASE set");


/**
 * @brief Frame size of a scope. The channel counts of the view it carries.
 *
 * @param scope     Scope of the link.
 * @param nAnalog   Number of analog channels of the view.
 * @param nDigital  Number of digital channels of the view.
 */

static void wireCounts( LinkScope scope, uint8_t& nAnalog, uint8_t& nDigital )
{
		// A link between nodes carries BASE, a link between boards SYSTEM
	if (scope == LinkScope::INTER_NODE) {
		nAnalog  = static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT);
		nDigital = static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT);
	} else {
		nAnalog  = static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT);
		nDigital = static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT);
	}
}


	/// This end sends frames.
static bool endTransmits( LinkEnd end ) { return end != LinkEnd::RX; }

	/// This end receives frames.
static bool endReceives ( LinkEnd end ) { return end != LinkEnd::TX; }


/**
 * @brief Port opener. Claims the hardware of a wired link and gives its transport.
 *
 * @details An end that does not transmit, or does not receive, leaves that pin
 *   unclaimed.  Each further kind of port adds one case here.
 *
 * @param map     Wiring of the link, already validated.
 * @param pinReg  Optional pin registry.
 *
 * @return The transport, or nullptr on failure.
 */

static NodeCom* openPort( const ComDevMap& map, PinReg* pinReg )
{
	const ComPort& port = *map.port;

		// One case per kind of port
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

/**
 * @brief Wiring validator. Refuses a bad description before any port is claimed.
 *
 * @details In order:
 *   1. Pointers, without which nothing else can be read.
 *   2. Coupling: a link ON_COUPLING needs its CouplingDev, and its table to pair.
 *   3. Direction: the type of the link and the end of this board must agree.
 *   4. Rates: 1000 / txHz is an integer period, so above 1000 Hz it falls to 0 and the
 *      link stays silent.  A receiving end needs a timeout to detect a loss.
 *   5. Line rate against the ceiling of the port.
 *   6. Table: between nodes only, and small enough for the RX buffers.
 */

bool combus_link_check( const ComDevMap& map )
{
		// 1. Pointers
	if (!map.dev || !map.port || !map.dev->protocol) {
		sys_log_err("[COMBUS_LINKS] wiring rejected — null device, port or protocol\n");
		return false;
	}

	const ComDev& dev = *map.dev;

		// 2. Coupling
	if (dev.activation == LinkActivation::ON_COUPLING && !dev.coupling) {
		sys_log_err("[COMBUS_LINKS] link '%s': a link ON_COUPLING needs its CouplingDev\n", dev.name);
		return false;
	}

	if (dev.activation == LinkActivation::ON_COUPLING && !dev.peer) {
		sys_log_err("[COMBUS_LINKS] link '%s': a link ON_COUPLING pairs through its coupler (ComDev::peer)\n", dev.name);
		return false;
	}

		// 3. Direction
	if (dev.type == LinkType::BIDIRECTIONAL && map.end != LinkEnd::TX_RX) {
		sys_log_err("[COMBUS_LINKS] link '%s': a bidirectional link must be wired TX_RX\n", dev.name);
		return false;
	}

	if (dev.type == LinkType::UNIDIRECTIONAL && map.end == LinkEnd::TX_RX) {
		sys_log_err("[COMBUS_LINKS] link '%s': a unidirectional link cannot be wired TX_RX\n", dev.name);
		return false;
	}

		// 4. Rates
	if (endTransmits(map.end) && dev.protocol->txHz == 0u) {
		sys_log_err("[COMBUS_LINKS] link '%s': a transmitting end needs a non-zero txHz\n", dev.name);
		return false;
	}

	if (endTransmits(map.end) && dev.protocol->txHz > 1000u) {
		sys_log_err("[COMBUS_LINKS] link '%s': above 1000 Hz, the period is 0 ms and the link stays silent\n", dev.name);
		return false;
	}

	if (endReceives(map.end) && dev.protocol->linkTimeoutMs == 0u) {
		sys_log_err("[COMBUS_LINKS] link '%s': a receiving end needs a non-zero linkTimeoutMs\n", dev.name);
		return false;
	}

		// 5. Line rate
	if (map.port->type == ComPortType::UART && dev.protocol->rate > map.port->uart.maxBaud) {
		sys_log_err("[COMBUS_LINKS] link '%s': rate %lu exceeds the ceiling of port '%s' (%lu)\n",
		            dev.name, (unsigned long)dev.protocol->rate,
		            map.port->name, (unsigned long)map.port->uart.maxBaud);
		return false;
	}

		// 6. Table
	if (dev.peer && dev.scope != LinkScope::INTER_NODE) {
		sys_log_err("[COMBUS_LINKS] link '%s': a coupler is for a link between nodes only\n", dev.name);
		return false;
	}

	if (dev.peer && (dev.peer->analog.count > sizeof(ComLinkRuntime::analogRx) / sizeof(uint16_t)
	                 || dev.peer->digital.count > sizeof(ComLinkRuntime::digitalRx) / sizeof(bool))) {
		sys_log_err("[COMBUS_LINKS] link '%s': the link through '%s' exceeds the RX buffers\n",
		            dev.name, dev.peer->coupler);
		return false;
	}

	return true;
}


/**
 * @brief Descriptor builder. Turns a validated wiring into what starts a link.
 *
 * @details In order:
 *   1. Validate.
 *   2. Frame size: the view of the scope, or the table of the link.
 *   3. Descriptor: TX and RX are filled according to the end of this board.  It is
 *      built apart, so that @p link is only written on success.
 *   4. What the watch of the link will need.
 */

bool combus_link_build( const ComDevMap& map,
                        NodeCom*         com,
                        ComLinkRuntime&  runtime,
                        ComBusLink&      link )
{
		// 1. Validate
	if (!com || !combus_link_check(map)) { return false; }

		// 2. Frame size
	const ComPeerLink* peer = map.dev->peer;
	uint8_t nAnalog  = 0u;
	uint8_t nDigital = 0u;

	wireCounts(map.dev->scope, nAnalog, nDigital);

		// Check for a table: its counts replace those of the view
	if (peer) {
		nAnalog  = peer->analog.count;
		nDigital = peer->digital.count;
	}

	const ComBusFrameCfg frameCfg = { nAnalog, nDigital };

		// 3. Descriptor
	ComBusLink built{};

	built.com          = com;
	built.layer        = linkScopeLayer(map.dev->scope);
	built.handshakeCtx = &runtime.handshake;
	built.name         = map.dev->name;
	built.peer         = peer;

		// Set the side that transmits
	if (endTransmits(map.end)) {
		built.txCfg = frameCfg;
		built.txHz  = map.dev->protocol->txHz;
	}

		// Set the side that receives
	if (endReceives(map.end)) {
		built.rxCfg      = frameCfg;
		built.analogBuf  = runtime.analogRx;
		built.digitalBuf = runtime.digitalRx;
	}

		// 4. What the watch needs
	runtime.dev                  = map.dev;
	runtime.com                  = com;
	runtime.watch.receives       = endReceives(map.end);
	runtime.watch.linkTimeoutMs  = map.dev->protocol->linkTimeoutMs;
	runtime.watch.startupGraceMs = map.dev->protocol->startupGraceMs;
	runtime.coupling.dev         = (map.dev->activation == LinkActivation::ON_COUPLING) ? map.dev->coupling : nullptr;
	runtime.coupling.role        = peer ? peer->role : ComLinkRole::SOCKET;

	link = built;

	return true;
}



// =============================================================================
// 3. INIT
// =============================================================================

/**
 * @brief Links bring-up. From the table of a board to links under watch.
 *
 * @details In order:
 *   1. Each map: validate before claiming the port, open it, build the link.  A failure
 *      leaves an empty slot, which the next steps skip: the other links still start.
 *      Frames are written to the target on the receiving links only.
 *   2. Pools, then each link.
 *   3. Watch of the links.
 */

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

		// 1. Each map
	bool allUp = true;

	for (uint8_t i = 0u; i < count; ++i) {
		storage.links[i] = ComBusLink{};

			// Check the wiring before claiming the port
		if (!combus_link_check(maps[i])) { allUp = false; continue; }

		NodeCom* com = openPort(maps[i], pinReg);

		if (!combus_link_build(maps[i], com, storage.runtime[i], storage.links[i])) {
			allUp = false;
			continue;
		}

			// Set the target of a receiving link
		if (storage.links[i].rxCfg.has_value()) { storage.links[i].target = target; }
	}

		// 2. Pools, then each link
	combus_tx_register_pool(storage.tx, count);
	combus_rx_register_pool(storage.rx, count);
	combus_links_start_all(storage.links, count);

		// 3. Watch
	combus_links_watch(storage.runtime, count);

	return allUp;
}


	/// Each link keeps its index: it is its slot in the TX and RX pools.
void combus_links_start_all( ComBusLink* links, uint8_t count )
{
	for (uint8_t i = 0u; i < count; ++i) {
		combus_link_start(i, &links[i]);
	}
}


/**
 * @brief Link start. Sets the contract of a link, then its TX and RX sides.
 *
 * @details In order:
 *   1. Guards.  An empty slot, left by a failure at build, has no transport.
 *   2. Expected MD5, by layer.  Resolved once here, then read by both sides.  Any other
 *      layer halts: a silent refusal would hide the bug.
 *   3. Context attached to both sides, before their init: combus_tx_init() arms the burst
 *      on it.
 *   4. Contract rules: a bidirectional end answers requests, waits for an ACK and sends
 *      no data while UNKNOWN.
 *   5. Next set: only a BASE link has its candidates apart.  A SYSTEM link carries them
 *      in its single set, so its next MD5 is the stable one.
 *   6. Table: a link between nodes has its own contract, the MD5 of its coupler and of a
 *      side.  This end announces its side and expects the other one.
 *   7. Candidates are told from the two MD5, and the contract starts UNKNOWN.
 *   8. TX side, then RX side, if present.
 */

void combus_link_start( uint8_t linkIdx, ComBusLink* link )
{
		// 1. Guards
	if (!link || !link->com) { return; }

	if (!link->handshakeCtx) {
		sys_log_err("[COMBUS_LINKS] start rejected — null handshakeCtx for link '%s'\n", link->name);
		return;
	}

		// 2. Expected MD5
	switch (link->layer) {
		case ChanLayer::BASE:
			link->handshakeCtx->expected.stable = combus::wire::kCombusBaseComBusMd5;
			break;
		case ChanLayer::SYSTEM:
			link->handshakeCtx->expected.stable = combus::wire::kCombusSystemComBusMd5;
			break;
		default:
			sys_log_err("[COMBUS_LINKS] FATAL: only BASE and SYSTEM travel on a link ('%s') — system halted\n",
			            link->name);
			while (1) { /* halt */ }
	}

		// 3. Context attached to both sides
	combus_tx_set_handshake_ctx(linkIdx, link->handshakeCtx);
	combus_rx_set_handshake_ctx(linkIdx, link->handshakeCtx);

		// 4. Contract rules
	link->handshakeCtx->name          = link->name;
	link->handshakeCtx->bidirectional = link->txCfg.has_value() && link->rxCfg.has_value();

		// 5. Next set
	link->handshakeCtx->baseSet       = (link->layer == ChanLayer::BASE);
	link->handshakeCtx->expected.next = link->handshakeCtx->baseSet
		? combus::wire::kCombusBaseComBusMd5Next
		: link->handshakeCtx->expected.stable;

		// 6. Table
	link->handshakeCtx->peer      = link->peer;
	link->handshakeCtx->announced = {};

	if (link->peer) {
		link->handshakeCtx->expected  = link->peer->expected;
		link->handshakeCtx->announced = link->peer->announced;
	}

		// 7. Candidates, and a contract that starts UNKNOWN
	link->handshakeCtx->hasCandidates =
		memcmp(link->handshakeCtx->expected.next, link->handshakeCtx->expected.stable, 16) != 0;

	combus_handshake_internal::clearContractValidated(link->handshakeCtx);

		// 8. TX side, then RX side
	if (link->txCfg) {
		combus_tx_init(linkIdx, link->com, *link->txCfg, link->txHz, link->layer);
	}

	if (link->rxCfg) {
		combus_rx_init(linkIdx, link->com, *link->rxCfg, link->analogBuf, link->digitalBuf, link->target, link->layer);
	}
}

// EOF combus_links.cpp
