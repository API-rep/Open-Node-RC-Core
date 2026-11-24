/******************************************************************************
 * @file  remote_map.h
 * @brief Mapper of a command source: the channels of the source → the inputs of the type.
 *
 * @details A mapper is a configuration, a simple switchboard: one row per input,
 *   `{ channel of the source, input, inversion }`.  Scaling belongs to the backend,
 *   framing (deadband, centre, expo) to the reframer, the behaviour of the machine to
 *   its chains.
 *
 *   The module reads the table in RAM only, never a table in flash.  The single door is
 *   remote_map_apply(): it checks the table, then replaces the one in RAM at once; a
 *   refused table leaves the old one in place.  An input without a row keeps its value
 *   (its safe value, written by the base while no source drives).
 *   Design: doc/WIP - Local remote.md §5.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/config/machines/combus_types.h>   // AnalogComBusID, DigitalComBusID, ComBus
#include "remote_source.h"                        // RemoteInputs



// =============================================================================
// 1. A TABLE
// =============================================================================

	/// Rows of a table in RAM: analog, digital.  A longer table is refused.
inline constexpr uint8_t kRemoteMapMaxAnalog  = 16u;
inline constexpr uint8_t kRemoteMapMaxDigital = 32u;


/**
 * @brief One analog row: a channel of the source to an input.
 */

struct RemoteAnalogRoute {
	AnalogComBusID from;       ///< channel of the source
	AnalogComBusID to;         ///< input of the type
	bool           inverted;   ///< value = max - value
};


/**
 * @brief One digital row: a channel of the source to an input.
 */

struct RemoteDigitalRoute {
	DigitalComBusID from;       ///< channel of the source
	DigitalComBusID to;         ///< input of the type
	bool            inverted;   ///< value = !value
};


/**
 * @brief The analog rows of a table.
 */

struct RemoteAnalogRoutes {
	const RemoteAnalogRoute* rows;
	uint8_t                  count;
};


/**
 * @brief The digital rows of a table.
 */

struct RemoteDigitalRoutes {
	const RemoteDigitalRoute* rows;
	uint8_t                   count;
};


/**
 * @brief A complete table, in flash: the one of the type, or of the instance.
 */

struct RemoteMapTable {
	const char*         name;      ///< tag for the log ("dumper_truck/ps4"...)
	RemoteAnalogRoutes  analog;
	RemoteDigitalRoutes digital;
};


/**
 * @brief The table in RAM, the only one the module reads.
 */

struct RemoteMapRam {
	const char*        name                          = nullptr;   ///< table retained; nullptr = none yet
	uint8_t            analogCount                   = 0u;
	uint8_t            digitalCount                  = 0u;
	RemoteAnalogRoute  analog[kRemoteMapMaxAnalog]   = {};
	RemoteDigitalRoute digital[kRemoteMapMaxDigital] = {};
};



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief The single door: check a table, then put it in RAM at once.
 *
 * @details The table is refused, with its cause in the log, when:
 *   - it is longer than the table in RAM;
 *   - a row reads a channel that is not on the bus;
 *   - a row writes a channel that is not an input of the type on this board;
 *   - two rows read the same channel of the source, or write the same input.
 *   Refused, the table in RAM does not change.
 *
 * @param ram     The table in RAM of the module.
 * @param table   The table to put in place.
 * @param inputs  The inputs of the type on this board.
 *
 * @return true if the table is in place.
 */

bool remote_map_apply( RemoteMapRam& ram, const RemoteMapTable& table, const RemoteInputs& inputs );


/**
 * @brief Write the inputs from the channels of the source, through the table in RAM.
 *
 * @details Written with the identity of the board.  An empty table writes nothing.
 *
 * @param ram  The table in RAM of the module.
 * @param bus  ComBus the channels are read from and the inputs written to.
 */

void remote_map_route( const RemoteMapRam& ram, ComBus& bus );

// EOF remote_map.h
