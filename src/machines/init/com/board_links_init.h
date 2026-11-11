/******************************************************************************
 * @file  board_links_init.h
 * @brief ComBus links of the board: from its table to running links.
 *
 * @details The board declares its links next to its ports (comLinks[] of its
 *   envCfg.h): a ComDev of the node wired on one of its ports.  The links
 *   themselves are started by combus_links_init() (combus_links.h).
 *****************************************************************************/
#pragma once



// =============================================================================
// 1. API
// =============================================================================

	/// Start the ComBus links of the board, from its table comLinks[].
void board_links_init();

// EOF board_links_init.h
