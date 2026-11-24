/******************************************************************************
 * @file  remote_module.h
 * @brief A command source as a module: backend, reframer, mapper.
 *
 * @details The layers of a module (doc/WIP - Local remote.md §4):
 *   - backend: reads the device, writes the channels of the source (already scaled),
 *     says if the source is connected;
 *   - reframer: a chain of the `source` stage, run right after each read that
 *     succeeded, on the channels of the source, which it overwrites in place;
 *   - mapper: the table in RAM (remote_map.h), from the channels of the source to the
 *     inputs of the type.
 *   The module plugs into the base (remote_source.h) through remote_module_poll() and
 *   remote_module_apply(), with a RemoteModule as context.  Its memory is the caller's.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <struct/combus_proc_struct.h>           // CbChain
#include <core/config/machines/combus_types.h>   // ComBus
#include "remote_source.h"                        // RemoteInputs
#include "remote_map.h"                           // RemoteMapTable, RemoteMapRam



// =============================================================================
// 1. RHYTHM
// =============================================================================

	/// Ceiling of the read rhythm of a module: the cost of the reframer is bounded by it.
inline constexpr uint32_t kRemoteSourceMaxHz = 100u;


	/// True if a read period keeps under the ceiling; for a static_assert at the declaration.
constexpr bool remote_period_ok( uint32_t periodMs )
{
	return periodMs >= (1000u / kRemoteSourceMaxHz);
}



// =============================================================================
// 2. A MODULE
// =============================================================================

/**
 * @brief The backend of a module: the device of the source.
 *
 * @details Contract of refresh(): it rewrites every channel of the source on each call,
 *   from the state of the device, so that the reframer always starts from the raw
 *   value.  Not connected, it writes their safe value and returns false.
 */

struct RemoteBackend {
	void  (*begin)(void* ctx);                  ///< start the device; nullptr = nothing to start
	bool  (*refresh)(ComBus& bus, void* ctx);   ///< read the device; true = connected
	void* ctx;                                  ///< state of the backend, owned by the caller
};


/**
 * @brief The reframer of a module: chains of the `source` stage, stateless processors.
 */

struct RemoteReframer {
	CbChain* chains;   ///< nullptr = no reframing
	uint8_t  count;
};


/**
 * @brief What a module is made of, in flash.
 */

struct RemoteModuleCfg {
	const char*           name;
	RemoteBackend         backend;
	uint32_t              periodMs;                ///< one read per period (remote_period_ok())
	RemoteReframer        reframer;

		// Mapper: the table of the instance replaces the one of the type
	const RemoteMapTable* typeMap;
	const RemoteMapTable* instanceMap = nullptr;   ///< nullptr = the table of the type
	const RemoteInputs*   inputs      = nullptr;   ///< inputs of the type on this board, for the checks
};


/**
 * @brief The state of a module, in RAM.
 */

struct RemoteModuleState {
	RemoteMapRam map        = {};
	bool         mapReady   = false;   ///< a table passed the checks: the source may drive
	bool         connected  = false;   ///< at the last read
	bool         everRead   = false;
	uint32_t     lastReadMs = 0u;
};


/**
 * @brief A module: its description and its state.  The context of its RemoteSourceDev.
 */

struct RemoteModule {
	const RemoteModuleCfg* cfg;
	RemoteModuleState*     state;
};



// =============================================================================
// 3. API
// =============================================================================

/**
 * @brief Start a module, once at boot.
 *
 * @details Starts the backend, then puts in RAM the table of the instance if it
 *   declares one, else the table of the type (remote_map_apply()).  A refused table:
 *   the source is refused, with its cause in the log, and is never alive.
 *
 * @param module  The module.
 *
 * @return true if the source is accepted.
 */

bool remote_module_begin( RemoteModule& module );


/**
 * @brief RemoteSourceDev::poll of a module. Reads the source once per period.
 *
 * @details When the period has elapsed (or on the first call), reads the backend and,
 *   only when the read succeeded, runs the reframer: once per sample, never between
 *   two.  A refused source is never alive.
 *
 * @param bus  ComBus the channels of the source are written to.
 * @param ctx  The RemoteModule.
 *
 * @return true if the source is connected, at its last read.
 */

bool remote_module_poll( ComBus& bus, void* ctx );


/**
 * @brief RemoteSourceDev::apply of a module. Its mapper writes the inputs.
 *
 * @param bus  ComBus the channels are read from and the inputs written to.
 * @param ctx  The RemoteModule.
 */

void remote_module_apply( ComBus& bus, void* ctx );


/**
 * @brief The door of the mapper of a module: check a table, then put it in place.
 *
 * @details remote_map_apply() on the table in RAM of the module.  Refused, the table in
 *   place stays, and so does the right of the source to drive.
 *
 * @param module  The module.
 * @param table   The table to put in place.
 *
 * @return true if the table is in place.
 */

bool remote_module_map_apply( RemoteModule& module, const RemoteMapTable& table );

// EOF remote_module.h
