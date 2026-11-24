/******************************************************************************
 * @file  remote_map.cpp
 * @brief Mapper of a command source: the channels of the source → the inputs of the type.
 *****************************************************************************/

#include "remote_map.h"

#include <core/system/combus/combus_access.h>   // combus_get_raw_analog(), combus_set_raw_analog(), combus_set_digital()
#include <core/system/debug/logging/debug.h>



// =============================================================================
// 1. CHECKS
// =============================================================================

	/// True if @p id is one of the analog inputs.
static bool isAnalogInput( const RemoteInputs& inputs, AnalogComBusID id )
{
		// Each analog input
	for (uint8_t i = 0u; i < inputs.analog.count; ++i) {
		if (inputs.analog.ids[i] == id) { return true; }
	}

	return false;
}


	/// True if @p id is one of the digital inputs.
static bool isDigitalInput( const RemoteInputs& inputs, DigitalComBusID id )
{
		// Each digital input
	for (uint8_t i = 0u; i < inputs.digital.count; ++i) {
		if (inputs.digital.ids[i] == id) { return true; }
	}

	return false;
}


/**
 * @brief Analog rows. Checks the bounds, the targets and the duplicates.
 *
 * @param table   The table to check.
 * @param inputs  The inputs of the type on this board.
 *
 * @return true if every analog row is valid.
 */

static bool analogRowsOk( const RemoteMapTable& table, const RemoteInputs& inputs )
{
	const uint8_t chCount = static_cast<uint8_t>(AnalogComBusID::CH_COUNT);

		// Each analog row
	for (uint8_t i = 0u; i < table.analog.count; ++i) {
		const RemoteAnalogRoute& r = table.analog.rows[i];

			// Check the channel read, then the input written
		if (static_cast<uint8_t>(r.from) >= chCount) {
			sys_log_err("[REMOTE_MAP] table '%s': analog row %u reads channel %u, not on the bus\n",
			            table.name, (unsigned)i, (unsigned)r.from);
			return false;
		}
		if (!isAnalogInput(inputs, r.to)) {
			sys_log_err("[REMOTE_MAP] table '%s': analog row %u writes channel %u, not an input of this board\n",
			            table.name, (unsigned)i, (unsigned)r.to);
			return false;
		}

			// Check the rows after it for the same channel or the same input
		for (uint8_t j = i + 1u; j < table.analog.count; ++j) {
			if (table.analog.rows[j].from == r.from || table.analog.rows[j].to == r.to) {
				sys_log_err("[REMOTE_MAP] table '%s': analog rows %u and %u share a channel or an input\n",
				            table.name, (unsigned)i, (unsigned)j);
				return false;
			}
		}
	}

	return true;
}


/**
 * @brief Digital rows. Checks the bounds, the targets and the duplicates.
 *
 * @param table   The table to check.
 * @param inputs  The inputs of the type on this board.
 *
 * @return true if every digital row is valid.
 */

static bool digitalRowsOk( const RemoteMapTable& table, const RemoteInputs& inputs )
{
	const uint8_t chCount = static_cast<uint8_t>(DigitalComBusID::CH_COUNT);

		// Each digital row
	for (uint8_t i = 0u; i < table.digital.count; ++i) {
		const RemoteDigitalRoute& r = table.digital.rows[i];

			// Check the channel read, then the input written
		if (static_cast<uint8_t>(r.from) >= chCount) {
			sys_log_err("[REMOTE_MAP] table '%s': digital row %u reads channel %u, not on the bus\n",
			            table.name, (unsigned)i, (unsigned)r.from);
			return false;
		}
		if (!isDigitalInput(inputs, r.to)) {
			sys_log_err("[REMOTE_MAP] table '%s': digital row %u writes channel %u, not an input of this board\n",
			            table.name, (unsigned)i, (unsigned)r.to);
			return false;
		}

			// Check the rows after it for the same channel or the same input
		for (uint8_t j = i + 1u; j < table.digital.count; ++j) {
			if (table.digital.rows[j].from == r.from || table.digital.rows[j].to == r.to) {
				sys_log_err("[REMOTE_MAP] table '%s': digital rows %u and %u share a channel or an input\n",
				            table.name, (unsigned)i, (unsigned)j);
				return false;
			}
		}
	}

	return true;
}



// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Single door. Checks the whole table first, then copies it at once.
 *
 * @details In order:
 *   1. The size: a table longer than the RAM is refused before any row is read.
 *   2. The rows, analog then digital (bounds, targets, duplicates).
 *   3. Only then the copy: a refused table never leaves half a table in RAM.
 */

bool remote_map_apply( RemoteMapRam& ram, const RemoteMapTable& table, const RemoteInputs& inputs )
{
	const char* name = table.name ? table.name : "?";

		// 1. Size
	if (table.analog.count > kRemoteMapMaxAnalog || table.digital.count > kRemoteMapMaxDigital) {
		sys_log_err("[REMOTE_MAP] table '%s': %u analog and %u digital rows, %u and %u at most\n", name,
		            (unsigned)table.analog.count, (unsigned)table.digital.count,
		            (unsigned)kRemoteMapMaxAnalog, (unsigned)kRemoteMapMaxDigital);
		return false;
	}

		// 2. Rows
	if (!analogRowsOk(table, inputs) || !digitalRowsOk(table, inputs)) {
		sys_log_err("[REMOTE_MAP] table '%s' refused, the previous one stays\n", name);
		return false;
	}

		// 3. Copy
	for (uint8_t i = 0u; i < table.analog.count; ++i)  { ram.analog[i]  = table.analog.rows[i]; }
	for (uint8_t i = 0u; i < table.digital.count; ++i) { ram.digital[i] = table.digital.rows[i]; }

	ram.analogCount  = table.analog.count;
	ram.digitalCount = table.digital.count;
	ram.name         = name;

	sys_log_info("[REMOTE_MAP] table '%s' in place (%u analog, %u digital)\n", name,
	             (unsigned)ram.analogCount, (unsigned)ram.digitalCount);

	return true;
}


	/// The inversion is done here, the only computation of the mapper.
void remote_map_route( const RemoteMapRam& ram, ComBus& bus )
{
		// Each analog row
	for (uint8_t i = 0u; i < ram.analogCount; ++i) {
		const RemoteAnalogRoute& r = ram.analog[i];
		const uint16_t           v = combus_get_raw_analog(bus, r.from);

		combus_set_raw_analog(bus, r.to, r.inverted ? static_cast<uint16_t>(bus.analogBusMaxVal - v) : v);
	}

		// Each digital row
	for (uint8_t i = 0u; i < ram.digitalCount; ++i) {
		const RemoteDigitalRoute& r = ram.digital[i];
		const bool                v = bus.digitalBus[static_cast<uint8_t>(r.from)].value;

		combus_set_digital(bus, r.to, v != r.inverted);
	}
}

// EOF remote_map.cpp
