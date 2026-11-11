/******************************************************************************
 * @file  combus_link_watch.cpp
 * @brief Watch of the running links: their state, their loss, and their safe values.
 *****************************************************************************/

#include "combus_link_watch.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/coupling/combus_coupling_watch.h>
#include <core/system/debug/logging/debug.h>
#include <core/system/combus/combus_access.h>   // combus_link_set_raw_analog(), combus_link_set_digital()
#include <core/system/combus/combus_res.h>      // CbusNeutral

#include "combus.h"   // AnalogComBusID, DigitalComBusID



// =============================================================================
// 1. STORAGE AND HELPERS
// =============================================================================

	// The links under watch, stored by the caller (see combus_links_watch()).
static ComLinkRuntime* s_watched      = nullptr;
static uint8_t         s_watchedCount = 0u;


/**
 * @brief Coupling frame bridge. Hands a frame received on a watched link to its pairing.
 *
 * @param linkIdx  Index of the link.
 * @param frame    Coupling frame received.
 */

static void onCouplingFrame( uint8_t linkIdx, const CouplingFrame& frame )
{
		// Check for a coupled link under watch
	if (!s_watched || linkIdx >= s_watchedCount || !s_watched[linkIdx].coupling.dev) { return; }

	ComLinkRuntime& rt = s_watched[linkIdx];

	combus_pairing_on_frame(rt.coupling.pairing, *rt.coupling.dev, rt.coupling.role, frame, millis());
}


/**
 * @brief Loss with a grace. Lost, or never seen once the startup grace has elapsed.
 *
 * @param linkIdx  Index of the link.
 *
 * @return true if the link counts as lost.
 */

static bool lostAfterGrace( uint8_t linkIdx )
{
	switch (combus_link_state(linkIdx)) {
		case ComLinkState::LOST:
			return true;
		case ComLinkState::NEVER_SEEN:
			return (millis() - s_watched[linkIdx].watch.watchStartMs) >= s_watched[linkIdx].watch.startupGraceMs;
		case ComLinkState::ALIVE:
		default:
			return false;
	}
}



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Start of the watch. Sets each link to a fresh state, and its grace to now.
 *
 * @details For each link, in order:
 *   1. The startup grace starts now, and nothing is reported lost yet.
 *   2. A coupled link starts off, until it is paired.
 *   3. A receiving link gives its timeout to the RX module, which needs it to ignore
 *      silent links in the both_or merge.
 */

void combus_links_watch( ComLinkRuntime* runtime, uint8_t count )
{
	s_watched      = runtime;
	s_watchedCount = count;

	combus_coupling_set_rx_handler(onCouplingFrame);

	const uint32_t now = millis();

		// Each link
	for (uint8_t i = 0u; i < count; ++i) {
			// 1. Fresh state
		runtime[i].watch.watchStartMs = now;
		runtime[i].watch.reportedLost = false;
		runtime[i].watch.wasAlive     = false;
		runtime[i].coupling.coupled   = false;
		runtime[i].coupling.pairing   = {};

			// 2. A coupled link starts off
		if (runtime[i].coupling.dev) {
			combus_tx_set_active(i, false);
			combus_rx_set_mode(i, CombusRxMode::OFF);
		}

			// 3. Timeout for the both_or merge
		if (runtime[i].watch.receives) { combus_rx_set_link_timeout(i, runtime[i].watch.linkTimeoutMs); }
	}
}


/**
 * @brief Watch pass. Follows the couplings, then the loss of each link.
 *
 * @details First the passive couplers.  Then, for each link, in order:
 *   1. Coupling: step the pairing, write the contributors.  An unhitched link stops here.
 *   2. A link that only transmits has nothing to watch.
 *   3. A link with a table that stops being alive: safe values, so that a relaying node
 *      passes on the safe value as it is.
 *   4. Log the loss and the recovery, once per change.
 */

void combus_links_update( ComBus& bus )
{
	combus_couplings_update(bus);

		// Each link
	for (uint8_t i = 0u; i < s_watchedCount; ++i) {
		ComLinkRuntime& rt = s_watched[i];

			// 1. Coupling
		if (rt.coupling.dev) {
			combus_coupling_follow(rt, i, bus);
			combus_coupling_publish(rt, bus, lostAfterGrace(i));

			if (!rt.coupling.coupled) { continue; }
		}

			// 2. Link that only transmits
		if (!rt.watch.receives) { continue; }

			// 3. Safe values on the loss
		const bool alive = (combus_link_state(i) == ComLinkState::ALIVE);

		if (rt.watch.wasAlive && !alive && rt.dev->peer) {
			combus_link_take_back_safe_values(*rt.dev->peer, bus);

			sys_log_warn("[COMBUS_LINKS] link '%s': received channels back to their safe value\n",
			             rt.dev->name ? rt.dev->name : "?");
		}

		rt.watch.wasAlive = alive;

			// 4. Log the changes
		const bool lost = lostAfterGrace(i);

		if (lost == rt.watch.reportedLost) { continue; }

		rt.watch.reportedLost = lost;

		if (lost) {
			sys_log_warn("[COMBUS_LINKS] link '%s' lost (%s)\n", rt.dev->name ? rt.dev->name : "?",
			             rt.dev->scope == LinkScope::INTRA_NODE ? "between boards" : "to another node");
		} else {
			sys_log_info("[COMBUS_LINKS] link '%s' back\n", rt.dev->name ? rt.dev->name : "?");
		}
	}
}


	/// A link never seen is not lost here: only combus_links_lost() and the grace decide that.
ComLinkState combus_link_state( uint8_t linkIdx )
{
	if (linkIdx >= s_watchedCount || !s_watched[linkIdx].watch.receives) { return ComLinkState::NEVER_SEEN; }

		// Check for a first frame, then for a recent one
	if (combus_rx_age_ms(linkIdx) == UINT32_MAX)                             { return ComLinkState::NEVER_SEEN; }
	if (combus_rx_is_alive(linkIdx, s_watched[linkIdx].watch.linkTimeoutMs)) { return ComLinkState::ALIVE; }

	return ComLinkState::LOST;
}


/**
 * @brief Loss by scope. Feeds NODE_LINK_FAIL and BOARD_LINK_FAIL.
 *
 * @details A link to another node counts at once, since that node may not be there.  A
 *   link between boards waits for the startup grace, since the boards start together.
 */

bool combus_links_lost( LinkScope scope )
{
		// Each link
	for (uint8_t i = 0u; i < s_watchedCount; ++i) {
		const ComLinkRuntime& rt = s_watched[i];

			// Check that this link counts: a coupled link has its own contributor
		if (!rt.watch.receives || rt.dev->scope != scope || rt.coupling.dev) { continue; }

		if (scope == LinkScope::INTER_NODE && combus_link_state(i) != ComLinkState::ALIVE) { return true; }
		if (scope == LinkScope::INTRA_NODE && lostAfterGrace(i))                           { return true; }
	}

	return false;
}


/**
 * @brief Safe values. Takes back what this node had received from a link now gone.
 *
 * @details The positions this node sends do not move.  A both_or position is skipped: a
 *   frame never writes it, and the merge ignores a link that is not alive.
 */

void combus_link_take_back_safe_values( const ComPeerLink& peer, ComBus& bus )
{
		// Each analog position this node receives
	for (uint8_t p = 0u; p < peer.analog.count; ++p) {
		if (!peer.analog.rx[p]) { continue; }

		combus_link_set_raw_analog(bus, static_cast<AnalogComBusID>(peer.analog.ids[p]), CbusNeutral, ChanLayer::BASE);
	}

		// Each digital position this node receives, both_or left out
	for (uint8_t p = 0u; p < peer.digital.count; ++p) {
		if (!peer.digital.rx[p] || bus.digitalBus[peer.digital.ids[p]].bothOr.enabled) { continue; }

		combus_link_set_digital(bus, static_cast<DigitalComBusID>(peer.digital.ids[p]), false, ChanLayer::BASE);
	}
}


	/// Pairing state read by the tests, without a whole snapshot.
PairingState combus_links_pairing_state( uint8_t linkIdx )
{
	if (!s_watched || linkIdx >= s_watchedCount || !s_watched[linkIdx].coupling.dev) { return PairingState::DETACHED; }

	return s_watched[linkIdx].coupling.pairing.state;
}



// =============================================================================
// 3. SNAPSHOTS FOR THE DASHBOARDS
// =============================================================================

	/// Nothing under watch counts as no link.
uint8_t combus_links_count( void )
{
	return s_watched ? s_watchedCount : 0u;
}


	/// A copy: the dashboards never read the internal structures of the links.
bool combus_links_view( uint8_t linkIdx, ComLinkView& out )
{
	if (!s_watched || linkIdx >= s_watchedCount) { return false; }

	const ComLinkRuntime& rt = s_watched[linkIdx];

	out                = ComLinkView{};

		// Check for a description: an empty slot keeps the defaults of the view
	if (rt.dev) {
		out.name  = rt.dev->name;
		out.scope = rt.dev->scope;
		out.peer  = rt.dev->peer;
	}

	out.receives       = rt.watch.receives;
	out.linkTimeoutMs  = rt.watch.linkTimeoutMs;
	out.startupGraceMs = rt.watch.startupGraceMs;
	out.state          = combus_link_state(linkIdx);
	out.contractStable = rt.handshake.contractValidated;
	out.nextMode       = rt.handshake.nextMode;
	out.rxAgeMs        = combus_rx_age_ms(linkIdx);
	out.coupling       = rt.coupling.dev;
	out.pairing        = rt.coupling.pairing;

	return true;
}

// EOF combus_link_watch.cpp
