/******************************************************************************
 * @file  remote_source.h
 * @brief Base of the command sources: one source alive drives the inputs of the type.
 *
 * @details A command source is a module of the board that acquires the commands
 *   (HAS_REMOTE): a PS4 controller, an SBUS or PPM receiver, the remote of the project.
 *   Each cycle, remote_sources_update() asks each source if it is alive:
 *   - exactly one alive: its mapper writes the inputs (*_STICK, *_BTN, BOARD channels);
 *   - none, or several: no mapper runs, the inputs go back to their safe value (stick
 *     at neutral, button released), and the loss is raised.  Two sources alive at once
 *     never drive the machine: the base cannot know which one the driver holds.
 *   The loss is NODE_LINK_FAIL, which the base writes on a HAS_REMOTE board.  The base
 *   holds no state of its own: the caller owns the sources and their watch.
 *   Design: doc/WIP - Local remote.md §2 and §4.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/config/machines/combus_types.h>   // AnalogComBusID, DigitalComBusID, ComBus



// =============================================================================
// 1. A SOURCE
// =============================================================================

/**
 * @brief One command source, as the base sees it.
 *
 * @details poll() is called on every cycle, for every source; apply() only for the
 *   source that drives, right after.  A source that is not alive keeps its hands off
 *   the inputs: the base writes their safe value.
 */

struct RemoteSourceDev {
	const char* name;                               ///< tag for the log
	bool        (*poll)(ComBus& bus, void* ctx);    ///< read the source; true = alive
	void        (*apply)(ComBus& bus, void* ctx);   ///< mapper: write the inputs from the source
	void*       ctx;                                ///< state of the module, owned by the caller
};



// =============================================================================
// 2. THE INPUTS
// =============================================================================

/**
 * @brief The analog inputs the base sends back to their safe value.
 */

struct RemoteAnalogInputs {
	const AnalogComBusID* ids;
	uint8_t               count;
};


/**
 * @brief The digital inputs the base sends back to their safe value.
 */

struct RemoteDigitalInputs {
	const DigitalComBusID* ids;
	uint8_t                count;
};


/**
 * @brief The inputs of the type on this board: those a mapper may write.
 */

struct RemoteInputs {
	RemoteAnalogInputs  analog;
	RemoteDigitalInputs digital;
};



// =============================================================================
// 3. THE BASE
// =============================================================================

	/// Upper bound of the sources of one board.  Beyond it, none is counted (loss raised).
inline constexpr uint8_t kRemoteSourceMaxCount = 8u;


/**
 * @brief How many sources are alive, at the last update.
 */

enum class RemoteSourceState : uint8_t {
	NONE,      ///< no source alive (or none declared): safe values, loss raised
	ONE,       ///< one source alive: it drives
	SEVERAL    ///< more than one alive: safe values, loss raised
};


/**
 * @brief What the base watches, kept by the caller between two updates.
 */

struct RemoteSourceWatch {
	RemoteSourceState state   = RemoteSourceState::NONE;   ///< at the last update
	uint8_t           active  = 0u;                        ///< index of the source that drives (state ONE)
	bool              started = false;                     ///< false until the first update: the first state is logged
};


/**
 * @brief The sources of a board, their inputs, and their watch.
 */

struct RemoteSources {
		// Configuration
	const RemoteSourceDev* devs   = nullptr;   ///< the sources; nullptr = none
	uint8_t                count  = 0u;
	RemoteInputs           inputs = {};

		// State
	RemoteSourceWatch      watch  = {};
};



// =============================================================================
// 4. API
// =============================================================================

/**
 * @brief Run the sources, once per cycle, before the input chains.
 *
 * @details Asks each source if it is alive, lets the only one alive write the inputs,
 *   or sends every input back to its safe value.  A change of state is logged once,
 *   with its cause.  Then, when the build has the contributor (HAS_FAILSAFE,
 *   HAS_NODE_LINK_FAILSAFE), writes NODE_LINK_FAIL: true unless exactly one source is
 *   alive, or when a link to another node is lost (combus_links_lost()).
 *
 * @param sources  The sources of the board, and their watch.
 * @param bus      ComBus the inputs and the contributor are written to.
 */

void remote_sources_update( RemoteSources& sources, ComBus& bus );

// EOF remote_source.h
