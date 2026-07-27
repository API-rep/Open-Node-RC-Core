/*!****************************************************************************
 * @file  proc_config.h
 * @brief Dumper truck — CbChain pipeline declaration (SIM layer, class-level).
 *
 * @details Exposes the SIM CbChain array consumed by sim_update() in the
 *   machine main loop.  Included from the vehicle header (IS_MAINBOARD block)
 *   so that board-specific envCfg.h can reference kSimChannels[] directly.
 *
 *   Channel pipelines (inCh -> procs -> outCh), execution order:
 *     SIM_THROTTLE : THROTTLE_BUS -> [ramp, drive-state, center, abs, scale,
 *                                      bypass] -> ESC_RPM_BUS  (wheel_speed)
 *     SIM_TRACTION : ESC_RPM_BUS  -> [upshift-damp, subgear-cap, gear-dir]
 *                                  -> ESC_SPEED_BUS
 *                    Reads wheel_speed BEFORE GEAR overwrites ESC_RPM_BUS.
 *     SIM_GEAR     : ESC_RPM_BUS  -> [gear-inv-ratio, gear-fsm, gear-ramp]
 *                                  -> GEAR
 *                    gear-inv-ratio converts wheel_speed to engine_rpm and
 *                    writes it to ESC_RPM_BUS (sound node reads engine RPM).
 *     SIM_STEERING : STEERING_BUS -> [bypass, ramp]  -> STEERING_RAMPED_BUS
 *     SIM_DUMP     : DUMP_BUS     -> [bypass, ramp]  -> DUMP_RAMPED_BUS
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
 * @details Each entry identifies one simulation pipeline (inCh -> procs -> outCh).
 *   Order matches execution order: TRACTION must run before GEAR so it reads
 *   wheel_speed (ESC_RPM_BUS) before gear_ratio_inv_fn overwrites it with engine_rpm.
 *   `SIM_CH_COUNT` is the sentinel used as array size and loop bound.
 */
enum SimCh {
    SIM_THROTTLE = 0,  ///< THROTTLE_BUS -> [center+abs+scale]           -> ESC_RPM_BUS (wheel_speed)
    SIM_TRACTION,      ///< ESC_RPM_BUS  -> [upshift-damp, subgear-cap, gear-dir] -> ESC_SPEED_BUS
    SIM_GEAR,          ///< ESC_RPM_BUS  -> [gear-inv-ratio, gear-fsm, gear-ramp] -> GEAR; side-effect: engine_rpm -> ESC_RPM_BUS
    SIM_STEERING,      ///< STEERING_BUS -> [bypass + ramp]          -> STEERING_RAMPED_BUS
    SIM_DUMP,          ///< DUMP_BUS     -> [bypass + ramp]          -> DUMP_RAMPED_BUS
    SIM_CH_COUNT       ///< Sentinel -- number of CbChain entries.
};


// =============================================================================
// 2. CHANNEL ARRAY
// =============================================================================

/// CbChain pipeline array — consumed by proc_chain_update() in the machine main loop.
extern CbChain kSimChannels[SIM_CH_COUNT];


// EOF proc_config.h
