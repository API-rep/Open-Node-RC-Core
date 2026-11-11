/******************************************************************************
 * @file com_init.h
 * @brief EnvCfg env communication transports initialisation — umbrella.
 *
 * @details Initialises the communication of the board.
 *
 *   Active transports:
 *     the ComBus links of the board table comLinks[] (envCfg.h)
 *
 *   Future:
 *     (additional transports registered here as sub-modules)
 *
 *   Called from output_init() after sys init.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. COM INIT
// =============================================================================

	/// Initialise all active communication transport layers.
void com_init();

// EOF com_init.h
