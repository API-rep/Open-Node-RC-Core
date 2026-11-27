/******************************************************************************
 * @file  input_sources.h
 * @brief The command sources of a machine board: its modules, run by the base.
 *
 * @details On the board that acquires the commands (HAS_REMOTE), each source is a
 *   module (core/system/input/remote_module.h): today the PS4 controller
 *   (HAS_REMOTE_PS4).  The base (core/system/input/remote_source.h) lets the only
 *   source alive write the inputs of the type, sends them back to their safe value
 *   otherwise, and writes NODE_LINK_FAIL.  A board without a source module has
 *   nothing to run: both calls do nothing.
 *   Design: doc/WIP - Local remote.md §2, §4.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>   // ComBus



// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Start the source modules of the board, once at boot.
 *
 * @details Each module starts its backend and puts its table in RAM; a refused table
 *   refuses the source, with the cause in the log.
 */

void input_sources_begin( void );


/**
 * @brief Run the sources, once per cycle, before the input chains.
 *
 * @param bus  ComBus of the node: the inputs and NODE_LINK_FAIL are written to it.
 */

void input_sources_update( ComBus& bus );


// EOF input_sources.h
