/*!****************************************************************************
 * @file  driven_machines.h
 * @brief The machines driven by this remote, and the link to each.
 *
 * @details One entry per driven machine: its name, its ComBus and the link to it.
 *   The remote drives one machine type (-D MACHINE_TYPE_<TYPE>): its ComBus is the
 *   BASE view of that type, mirrored by the ComBus Builder (what the machine
 *   receives, the remote sends).
 *
 *   The link carries only what the remote sends: its own table and MD5
 *   (combus::peer::kRemoteLink), the same whether the machine has couplers or not;
 *   no pairing.
 *
 *   TODO(chantier 4): several machines only fill this table, with one ComBus per
 *   machine type (generated namespaces).
 *******************************************************************************///
#pragma once

#include <core/system/combus/protocol/link/combus_link_defs.h>   // ComDev, ComDevMap, ComProtocolCfg
#include <core/system/hw/node_com.h>                             // ComPort
#include <combus.h>                                              // generated: comBus, the mirrored BASE view
#include <combus_peer_links.h>                                   // generated: combus::peer::kRemoteLink



	/// Protocol of the link to a machine: line rate, transmit rate, timeout, startup grace.
inline constexpr ComProtocolCfg kComProtoMachine = { 115200u, 50u, 250u, 3000u };

	/// The link to a machine: between nodes, both ways, its own table, no pairing.
inline constexpr ComDev kComDevMachine = {
	"machine", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
	&kComProtoMachine, LinkActivation::ALWAYS, nullptr, &combus::peer::kRemoteLink
};

	/// The port of the remote board to the machine: UART2, GPIO 17 TX / 16 RX.
	/// TODO(chantier 4): from a board configuration of the remote.
inline const ComPort kPortMachine = ComPort::makeUart("machine", { 2, 17, 16, 115200u });


/**
 * @brief A machine driven by this remote.
 */

struct DrivenMachine {
	const char*    name;   ///< for the logs: the machine type
	ComBus*        bus;    ///< its ComBus, one per machine type
	ComDevMap      link;   ///< the link to it
};

	/// The driven machines: one today.
inline const DrivenMachine kDrivenMachines[] = {
	{ "dumper_truck", &comBus, { &kComDevMachine, &kPortMachine, LinkEnd::TX_RX } },
};

inline constexpr uint8_t kDrivenMachineCount = sizeof(kDrivenMachines) / sizeof(kDrivenMachines[0]);

// EOF driven_machines.h
