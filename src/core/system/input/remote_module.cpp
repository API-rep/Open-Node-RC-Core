/******************************************************************************
 * @file  remote_module.cpp
 * @brief A command source as a module: backend, reframer, mapper.
 *****************************************************************************/

#include "remote_module.h"

#include <Arduino.h>   // millis()

#include <core/system/combus/processors/proc_chain.h>   // proc_chain_update()
#include <core/system/debug/logging/debug.h>



// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Boot of a module. The backend, then the table retained.
 *
 * @details In order:
 *   1. Start the backend.
 *   2. Retain the table of the most customised level that declares one: the instance,
 *      else the type.  No fusion: the table retained is complete.
 *   3. Put it through the door; refused, the source is refused.
 */

bool remote_module_begin( RemoteModule& module )
{
	const RemoteModuleCfg& cfg = *module.cfg;
	RemoteModuleState&     st  = *module.state;

		// 1. Backend
	if (cfg.backend.begin) { cfg.backend.begin(cfg.backend.ctx); }

		// 2. Table retained
	const RemoteMapTable* table = cfg.instanceMap ? cfg.instanceMap : cfg.typeMap;

		// Check for a table and the inputs to check it against
	if (!table || !cfg.inputs) {
		sys_log_err("[REMOTE_SOURCE] source '%s' refused: no table, or no inputs\n", cfg.name);
		st.mapReady = false;
		return false;
	}

		// 3. Door
	st.mapReady = remote_map_apply(st.map, *table, *cfg.inputs);

	if (!st.mapReady) { sys_log_err("[REMOTE_SOURCE] source '%s' refused\n", cfg.name); }

	return st.mapReady;
}


/**
 * @brief Read pass. One read per period; the reframer after a read that succeeded.
 *
 * @details In order:
 *   1. A refused source is never alive: no read either.
 *   2. Within the period, nothing is read: the last state stands.
 *   3. Read the backend; connected, run the reframer on the fresh channels.  The
 *      subtraction of millis() values is safe across the wrap.
 */

bool remote_module_poll( ComBus& bus, void* ctx )
{
	RemoteModule&          module = *static_cast<RemoteModule*>(ctx);
	const RemoteModuleCfg& cfg    = *module.cfg;
	RemoteModuleState&     st     = *module.state;

		// 1. Refused source
	if (!st.mapReady) { return false; }

		// 2. Period
	const uint32_t now = millis();

	if (st.everRead && (now - st.lastReadMs) < cfg.periodMs) { return st.connected; }

		// 3. Read, then reframe a fresh sample
	st.connected  = cfg.backend.refresh(bus, cfg.backend.ctx);
	st.lastReadMs = now;
	st.everRead   = true;

	if (st.connected && cfg.reframer.chains) { proc_chain_update(cfg.reframer.chains, cfg.reframer.count, bus); }

	return st.connected;
}


	/// The mapper reads the channels the last read (and the reframer) left.
void remote_module_apply( ComBus& bus, void* ctx )
{
	const RemoteModule& module = *static_cast<RemoteModule*>(ctx);

	remote_map_route(module.state->map, bus);
}


	/// The previous table and the right to drive stay when the table is refused.
bool remote_module_map_apply( RemoteModule& module, const RemoteMapTable& table )
{
		// Check for the inputs to check the table against
	if (!module.cfg->inputs) { return false; }

	const bool ok = remote_map_apply(module.state->map, table, *module.cfg->inputs);

	if (ok) { module.state->mapReady = true; }

	return ok;
}

// EOF remote_module.cpp
