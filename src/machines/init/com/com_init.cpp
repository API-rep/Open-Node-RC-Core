/******************************************************************************
 * @file com_init.cpp
 * @brief EnvCfg env communication transports initialisation — orchestrator.
 *
 * @details Starts the communication of the board:
 *     the ComBus links of its table comLinks[] (board_links_init.h).
 *
 *   Future:
 *     (add new transport sub-inits here)
 *****************************************************************************/

#include "com_init.h"
#include "board_links_init.h"


// =============================================================================
// 1. COM INIT ORCHESTRATOR
// =============================================================================

void com_init()
{
    board_links_init();
}

// EOF com_init.cpp
