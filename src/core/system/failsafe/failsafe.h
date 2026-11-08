/******************************************************************************
 * @file  failsafe.h
 * @brief Failsafe module: the hard chain, which leads to HALT.
 *
 * @details The module has two chains of the same shape: a reset, then one OR per
 *   contributor.
 *   - the hard chain (here, failsafe_chain.h) publishes FAILSAFE_HARD, which leads
 *     to HALT, held until a reboot;
 *   - the soft chain (failsafe_soft_chain.h) publishes FAILSAFE_SOFT, which leads
 *     to IDLE.
 *
 *   The module only publishes these two channels.  The reaction belongs to the
 *   machine: its runlevel chain turns them into a run level, and main.cpp stops the
 *   drivers in HALT.  See failsafe_module.md.
 *
 *   The module needs HAS_FAILSAFE, which declares FAILSAFE_HARD.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>   // ComBus



// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Run the hard chain once: publishes FAILSAFE_HARD.
 *
 * @details Two rules, both kept by the caller and the chain:
 *   - every source writes its contributor before this call: sys_manager_update()
 *     calls it after the inputs and vbat_update();
 *   - every contributor runs every cycle: the runner skips the processors after a
 *     claim, so no processor of this chain ever claims it (see cb_or.h).
 *
 * @param bus  ComBus of the node.
 */

void failsafe_update(ComBus& bus);

// EOF failsafe.h
