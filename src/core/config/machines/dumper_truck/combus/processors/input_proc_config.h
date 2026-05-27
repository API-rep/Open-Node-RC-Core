/*!****************************************************************************
 * @file  input_proc_config.h
 * @brief Dumper truck — INPUT chain declarations (class-level, button processors).
 *
 * @details Exposes the INPUT CbChain array consumed by the first runner
 *   in the machine main loop.  Included from the vehicle header (IS_MAINBOARD
 *   block) so that board-specific envCfg.h can reference kInputChains[] directly.
 *
 *   INPUT pipelines (optInCh → procs → outCh):
 *     INPUT_SUBGEAR      : SUBGEAR_BUS      → [toggle(SUBGEAR_SET_BTN),
 *                                              inc(GEAR_UP_BTN),
 *                                              dec(GEAR_DOWN_BTN)] → SUBGEAR_BUS
 *     INPUT_DIRECT_DRIVE : DIRECT_DRIVE_BTN → [toggle] → DIRECT_DRIVE
 *
 *   Each proc reads a digital button, modifies the channel value, and passes
 *   it to the next proc.  Final runner commits to the same output channel.
 *
 *   Processed BEFORE proc_chain_update() — button state changes are immediately
 *   visible to the gear FSM in the SIM chain.
 *******************************************************************************
 */
#pragma once

#include <struct/combus_proc_struct.h>   // CbChain


// =============================================================================
// 1. CHANNEL COUNT
// =============================================================================

/**
 * @brief Indices into `kInputChains[]`.
 *
 * @details Each entry identifies one INPUT pipeline (button → counter → digital state).
 *   `INPUT_CH_COUNT` is the sentinel used as array size and loop bound.
 */
enum InputCh {
    INPUT_SUBGEAR = 0,      ///< SUBGEAR_BUS → [toggle+inc+dec] → SUBGEAR_BUS
    INPUT_DIRECT_DRIVE = 1, ///< DIRECT_DRIVE_BTN → [toggle] → DIRECT_DRIVE
    INPUT_CH_COUNT          ///< Sentinel — number of CbChain entries.
};


// =============================================================================
// 2. CHANNEL ARRAY
// =============================================================================

/// CbChain pipeline array — consumed by first runner in the machine main loop.
extern CbChain kInputChains[INPUT_CH_COUNT];


// EOF input_proc_config.h
