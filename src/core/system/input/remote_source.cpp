/******************************************************************************
 * @file  remote_source.cpp
 * @brief Base of the command sources: one source alive drives the inputs of the type.
 *****************************************************************************/

#include "remote_source.h"

#include <core/system/combus/combus_access.h>   // combus_set_raw_analog(), combus_set_digital()
#include <core/system/debug/logging/debug.h>

#if defined(HAS_FAILSAFE) && defined(HAS_NODE_LINK_FAILSAFE)
#include <core/system/combus/protocol/link/combus_link_supervisor.h>   // combus_links_lost()
#endif



// =============================================================================
// 1. HELPERS
// =============================================================================

/**
 * @brief Safe values. Sends every input back to its declared safe value.
 *
 * @details The safe value is the `value:` of the .cb (neutral for a stick, low for a
 *   button): no action, no state changes.  Written with the identity of the board, like
 *   a mapper.
 *
 * @param inputs  The inputs of the type on this board.
 * @param bus     ComBus the inputs are written to.
 */

static void takeBackSafeValues( const RemoteInputs& inputs, ComBus& bus )
{
		// Each analog input
	for (uint8_t i = 0u; i < inputs.analog.count; ++i) {
		const AnalogComBusID id = inputs.analog.ids[i];
		combus_set_raw_analog(bus, id, bus.analogBus[static_cast<uint8_t>(id)].safe);
	}

		// Each digital input
	for (uint8_t i = 0u; i < inputs.digital.count; ++i) {
		const DigitalComBusID id = inputs.digital.ids[i];
		combus_set_digital(bus, id, bus.digitalBus[static_cast<uint8_t>(id)].safe);
	}
}


/**
 * @brief Log of a change. Says which source drives, or why none does.
 *
 * @param sources  The sources of the board, their watch already updated.
 * @param alive    Alive state of each source at this update, one entry per source.
 */

static void logChange( const RemoteSources& sources, const bool* alive )
{
	switch (sources.watch.state) {
		case RemoteSourceState::ONE:
			sys_log_info("[REMOTE_SOURCE] source '%s' drives\n", sources.devs[sources.watch.active].name);
			break;

		case RemoteSourceState::SEVERAL:
			sys_log_warn("[REMOTE_SOURCE] several sources alive, driving blocked:\n");

				// Name each source alive
			for (uint8_t i = 0u; i < sources.count; ++i) {
				if (alive[i]) { sys_log_warn("[REMOTE_SOURCE]   '%s'\n", sources.devs[i].name); }
			}
			break;

		case RemoteSourceState::NONE:
		default:
				// Check for too many sources: none is counted
			if (sources.count > kRemoteSourceMaxCount) {
				sys_log_err("[REMOTE_SOURCE] %u sources, %u at most: none counted\n",
				            (unsigned)sources.count, (unsigned)kRemoteSourceMaxCount);
				break;
			}

			sys_log_warn("[REMOTE_SOURCE] no source alive\n");
			break;
	}
}



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Sources pass. One source alive drives; otherwise safe values and the loss.
 *
 * @details In order:
 *   1. Ask each source if it is alive.  Every source is asked, even once a second one
 *      is found: each module keeps its own state up to date.
 *   2. Exactly one alive: its mapper writes the inputs.  None or several: the safe
 *      values, so that nothing moves while the driver has no single source.
 *   3. Log the change of state, or of the source that drives, once.
 *   4. The contributor NODE_LINK_FAIL, written on every cycle: lost unless exactly one
 *      source drives; a link to another node that is lost counts too, since this base
 *      is the only writer of the contributor on a board that acquires the commands.
 */

void remote_sources_update( RemoteSources& sources, ComBus& bus )
{
	bool    alive[kRemoteSourceMaxCount] = {};
	uint8_t aliveCount                   = 0u;
	uint8_t active                       = 0u;

	const uint8_t count = (sources.devs && sources.count <= kRemoteSourceMaxCount) ? sources.count : 0u;

		// 1. Alive sources
	for (uint8_t i = 0u; i < count; ++i) {
		alive[i] = sources.devs[i].poll(bus, sources.devs[i].ctx);

			// Count it, and remember the first one
		if (alive[i]) {
			if (aliveCount == 0u) { active = i; }
			++aliveCount;
		}
	}

	const RemoteSourceState state = (aliveCount == 0u) ? RemoteSourceState::NONE
	                              : (aliveCount == 1u) ? RemoteSourceState::ONE
	                                                   : RemoteSourceState::SEVERAL;

		// 2. The source that drives, or the safe values
	if (state == RemoteSourceState::ONE) {
		sources.devs[active].apply(bus, sources.devs[active].ctx);
	} else {
		takeBackSafeValues(sources.inputs, bus);
	}

		// 3. Log the change
	const bool changed = !sources.watch.started
	                  || state != sources.watch.state
	                  || (state == RemoteSourceState::ONE && active != sources.watch.active);

	sources.watch.state   = state;
	sources.watch.active  = active;
	sources.watch.started = true;

	if (changed) { logChange(sources, alive); }

		// 4. Contributor of the loss
#if defined(HAS_FAILSAFE) && defined(HAS_NODE_LINK_FAILSAFE)
	const bool lost = (state != RemoteSourceState::ONE) || combus_links_lost(LinkScope::INTER_NODE);
	combus_set_digital(bus, DigitalComBusID::NODE_LINK_FAIL, lost);
#endif
}

// EOF remote_source.cpp
