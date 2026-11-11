/******************************************************************************
 * @file  combus_coupling_watch.cpp
 * @brief Follow-up of the couplings: from the state of a pairing to the state of a link.
 *****************************************************************************/

#include "combus_coupling_watch.h"

#include <Arduino.h>  // millis()

#include <core/system/combus/protocol/link/combus_link_watch.h>   // combus_link_take_back_safe_values()
#include <core/system/combus/combus_access.h>                     // combus_set_digital()
#include <core/system/debug/logging/debug.h>

#include "combus.h"   // DigitalComBusID



// =============================================================================
// 1. LINK ON_COUPLING
// =============================================================================

/**
 * @brief Coupling follower. Applies a change of pairing state to the link.
 *
 * @details In order:
 *   1. Chain of the coupler, then the trigger it writes, then the pairing.
 *   2. No change: nothing else to do.
 *   3. COUPLED: new startup grace, peer given to the transport, then reception and
 *      transmission back on.  A wire has no setPeer.
 *   4. Otherwise: the transport forgets the peer.  A link that was alive takes back its
 *      safe values.  Then the link goes off; while PAIRING, the coupling frames still pass.
 */

void combus_coupling_follow( ComLinkRuntime& rt, uint8_t linkIdx, ComBus& bus )
{
		// 1. Chain, trigger, pairing
	const uint32_t now = millis();

	if (rt.coupling.dev->chain) { rt.coupling.dev->chain(bus); }

	const bool trig = bus.digitalBus[static_cast<uint8_t>(rt.coupling.dev->trigger)].value;
	const PairingState was = rt.coupling.pairing.state;
	const PairingState is  = combus_pairing_update(rt.coupling.pairing, *rt.coupling.dev, rt.coupling.role,
	                                               trig, rt.com, now);

		// 2. No change
	if (is == was) { return; }

	sys_log_info("[COMBUS_LINKS] link '%s' %s -> %s (%s)\n", rt.dev->name ? rt.dev->name : "?",
	             combus_pairing_state_name(was), combus_pairing_state_name(is), rt.coupling.dev->name);

	rt.coupling.coupled = (is == PairingState::COUPLED);

		// 3. COUPLED: the link is on
	if (is == PairingState::COUPLED) {
		rt.watch.watchStartMs = now;
		rt.watch.reportedLost = false;

			// The transport may now keep the frames of the paired peer only.
			// TODO(chantier 5, ESP-NOW): effective with the ESP-NOW adapter.
		if (rt.com && rt.com->setPeer) {
			const NodeAddr& addr = rt.coupling.pairing.peer;

			rt.com->setPeer(rt.com->ctx, addr.bytes, sizeof(addr.bytes));
		}

		combus_rx_set_mode(linkIdx, CombusRxMode::ON);
		combus_tx_set_active(linkIdx, true);

		return;
	}

		// 4. Otherwise: the link is off
	if (was == PairingState::COUPLED && rt.com && rt.com->setPeer) {
		rt.com->setPeer(rt.com->ctx, nullptr, 0u);
	}

		// Check for a link that was alive: its received values are no longer valid
	if (was == PairingState::COUPLED && rt.watch.wasAlive && rt.dev->peer) {
		combus_link_take_back_safe_values(*rt.dev->peer, bus);
	}

	rt.watch.wasAlive     = false;
	rt.watch.reportedLost = false;

	combus_tx_set_active(linkIdx, false);
	combus_rx_set_mode(linkIdx, (is == PairingState::PAIRING) ? CombusRxMode::PAIRING : CombusRxMode::OFF);
}


/**
 * @brief Contributors of a coupler. Tell the machine how the coupling is going.
 *
 * @details In order:
 *   1. `fail`, only while hitched.  PAIRING is sound: the outcome is not known yet.  A
 *      link that only transmits cannot be lost.
 *   2. `coupled`: COUPLED, and the detector settled when there is one.
 */

void combus_coupling_publish( ComLinkRuntime& rt, ComBus& bus, bool lost )
{
	const CouplingDev& dev = *rt.coupling.dev;

		// 1. Failsafe contributor
	if (dev.fail) {
		const PairingState st = rt.coupling.pairing.state;
		const bool fault = rt.coupling.pairing.engaged
			&& (st == PairingState::CONFLICT || st == PairingState::FAILED
			    || (st == PairingState::COUPLED && rt.watch.receives && lost));

		combus_set_digital(bus, *dev.fail, fault);
	}

		// 2. Coupled
	if (dev.coupled) {
		const bool detected = !dev.detector
			|| combus_pairing_detector(rt.coupling.pairing, dev,
			                           bus.digitalBus[static_cast<uint8_t>(*dev.detector)].value, millis());

		combus_set_digital(bus, *dev.coupled,
		                   rt.coupling.pairing.state == PairingState::COUPLED && detected);
	}
}



// =============================================================================
// 2. PASSIVE COUPLER
// =============================================================================

	// The passive couplers, stored by the caller (see combus_couplings_watch()).
static ComCouplingRuntime* s_couplings     = nullptr;
static uint8_t             s_couplingCount = 0u;


	/// Every coupler starts from a fresh state.
void combus_couplings_watch( ComCouplingRuntime* couplings, uint8_t count )
{
	s_couplings     = couplings;
	s_couplingCount = count;

	for (uint8_t i = 0u; i < count; ++i) { couplings[i].pairing = {}; }
}


/**
 * @brief Passive coupler pass. Its trigger is its detector, so there is nobody to pair with.
 *
 * @details For each coupler, in order: chain, debounced trigger, `coupled` written.
 */

void combus_couplings_update( ComBus& bus )
{
		// Each coupler
	for (uint8_t i = 0u; i < s_couplingCount; ++i) {
		ComCouplingRuntime& c = s_couplings[i];

		if (!c.dev) { continue; }

			// Chain, then trigger
		if (c.dev->chain) { c.dev->chain(bus); }

		const bool trig = bus.digitalBus[static_cast<uint8_t>(c.dev->trigger)].value;
		const PairingState st = combus_pairing_update_passive(c.pairing, *c.dev, trig, millis());

			// Write the state, if the coupler publishes it
		if (c.dev->coupled) { combus_set_digital(bus, *c.dev->coupled, st == PairingState::COUPLED); }
	}
}


	/// DETACHED out of range: an unknown coupler is not coupled.
PairingState combus_couplings_state( uint8_t idx )
{
	return (s_couplings && idx < s_couplingCount) ? s_couplings[idx].pairing.state : PairingState::DETACHED;
}


	/// Empty out of range: type 0 means unknown.
CouplingId combus_couplings_id( uint8_t idx )
{
	return (s_couplings && idx < s_couplingCount) ? s_couplings[idx].pairing.id : CouplingId{};
}



// =============================================================================
// 3. SNAPSHOTS FOR THE DASHBOARDS
// =============================================================================

	/// Nothing under watch counts as no coupler.
uint8_t combus_couplings_count( void )
{
	return s_couplings ? s_couplingCount : 0u;
}


	/// A copy: the dashboards never read the internal structures of the couplers.
bool combus_couplings_view( uint8_t idx, ComCouplingRuntime& out )
{
	if (!s_couplings || idx >= s_couplingCount) { return false; }

	out = s_couplings[idx];

	return true;
}

// EOF combus_coupling_watch.cpp
