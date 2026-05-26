/*!****************************************************************************
 * @file  sim_proc_config.h
 * @brief Dumper truck — CbChain pipeline declaration (SIM layer, class-level).
 *
 * @details Exposes the SIM CbChain array consumed by sim_update() in the
 *   machine main loop.  Included from the vehicle header (IS_MAINBOARD block)
 *   so that board-specific envCfg.h can reference kSimChannels[] directly.
 *
 *   Channel pipelines (optInCh → procs → outCh):
 *     SIM_THROTTLE : THROTTLE_BUS → [ramp, drive-state, center, abs, scale,
 *                                     bypass, ratio] → RPM_BUS
 *     SIM_GEAR     : RPM_BUS      → [bypass, gear-fsm, gear-ramp] → GEAR
 *     SIM_TRACTION : RPM_BUS      → [rpm_to_speed] → ESC_SPEED_BUS
 *     SIM_STEERING : STEERING_BUS → [bypass, ramp]  → STEERING_RAMPED_BUS
 *     SIM_DUMP     : DUMP_BUS     → [bypass, ramp]  → DUMP_RAMPED_BUS
 *
 *   SUBGEAR_BUS is written by INPUT chain (cb_btn procs) — see input_proc_config.h.
 *******************************************************************************
 */
#pragma once

#include <struct/combus_proc_struct.h>   // CbChain


// =============================================================================
// 1. CHANNEL COUNT
// =============================================================================

/**
 * @brief Indices into `kSimChannels[]`.
 *
 * @details Each entry identifies one simulation pipeline (inCh → procs → outCh).
 *   `SIM_CH_COUNT` is the sentinel used as array size and loop bound.
 */
enum SimCh {
    SIM_THROTTLE = 0,  ///< THROTTLE_BUS → [center+abs+scale+ratio] → RPM_BUS
    SIM_GEAR,          ///< RPM_BUS       → [gear-fsm]               → GEAR
    SIM_TRACTION,      ///< RPM_BUS       → [rpm_to_speed]           → ESC_SPEED_BUS
    SIM_STEERING,      ///< STEERING_BUS  → [bypass + ramp]          → STEERING_RAMPED_BUS
    SIM_DUMP,          ///< DUMP_BUS      → [bypass + ramp]          → DUMP_RAMPED_BUS
    SIM_CH_COUNT       ///< Sentinel — number of CbChain entries.
};


// =============================================================================
// 2. CHANNEL ARRAY
// =============================================================================

/// CbChain pipeline array — consumed by proc_chain_update() in the machine main loop.
extern CbChain kSimChannels[SIM_CH_COUNT];


// EOF sim_proc_config.h
