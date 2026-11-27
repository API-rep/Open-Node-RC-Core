/******************************************************************************
 * @file  sys_manager.h
 * @brief System step of a machine: every recurring system service, once per loop.
 *
 * @details One call per loop, before the application logic, in a fixed order: each
 *   step reads what the previous ones wrote in the same cycle.
 *     0. Reception: com_rx_update() applies the received frames and writes the link
 *        contributors, BOARD_LINK_FAIL, and NODE_LINK_FAIL on a board without a
 *        command source module.
 *     1. Inputs: input_sources_update(): the command sources of the board, through
 *        the base; on a board with a source module, the base writes NODE_LINK_FAIL.
 *     2. Battery: vbat_update() writes VBAT_FAIL.
 *     3. Hard chain: failsafe_update() publishes FAILSAFE_HARD.
 *     4. Soft chain: failsafe_soft_update() publishes FAILSAFE_SOFT.
 *
 *   The two failsafe channels are only published here.  The runlevel chain of the
 *   machine turns them into HALT or IDLE, and main.cpp reacts to the run level.
 *
 *   This file lives under machines/, not core/: step 1 calls the sources of the
 *   machine board (input_sources.h).  Each machine therefore owns this orchestrator.
 *   If several machines come to share it, the generic steps could move to core
 *   behind a registered input hook.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>



// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Run the system step once: reception, inputs, battery, the two failsafe chains.
 *
 * @details Call once per loop, as the first statement of loop(), before the
 *   application logic.
 *
 * @param bus  ComBus of the node.
 */

void sys_manager_update(ComBus& bus);

// EOF sys_manager.h
