/*!****************************************************************************
 * @file    sim_config.h
 * @brief   Volvo A60H Bruder — SimChannel pipeline declaration.
 *
 * @details Exposes the SimChannel array consumed by sim_update() in the
 *   machine main loop.
 *
 *   Channel pipelines:
 *     SIM_THROTTLE : THROTTLE_BUS  →  [center + abs + scale + ratio]  →  RPM_BUS (wire)
 *     SIM_GEAR     : RPM_BUS       →  [gear-fsm]                      →  GEAR    (wire)
 *     SIM_TRACTION : THROTTLE_BUS  →  [bypass + ramp]  →  ESC_SPEED_BUS (wire, motors)
 *     SIM_STEERING : STEERING_BUS  →  [bypass + ramp]  →  STEERING_RAMPED_BUS (local)
 *     SIM_DUMP     : DUMP_BUS      →  [bypass + ramp]  →  DUMP_RAMPED_BUS     (local)
 *******************************************************************************
 */
#pragma once

#include <struct/simulation_struct.h>   // SimChannel


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
    SIM_THROTTLE = 0,  ///< THROTTLE_BUS → [center+abs+scale+ratio] → RPM_BUS (wire, engine RPM)
    SIM_GEAR,          ///< RPM_BUS       → [gear-fsm]               → GEAR    (wire)
    SIM_TRACTION,      ///< THROTTLE_BUS  → [bypass + ramp]          → ESC_SPEED_BUS (wire, motors)
    SIM_STEERING,      ///< STEERING_BUS  → [bypass + ramp]          → STEERING_RAMPED_BUS
    SIM_DUMP,          ///< DUMP_BUS      → [bypass + ramp]          → DUMP_RAMPED_BUS
    SIM_CH_COUNT       ///< Sentinel — number of SimChannel entries.
};


// =============================================================================
// 2. CHANNEL ARRAY
// =============================================================================

/// SimChannel pipeline array — consumed by sim_update() in the machine main loop.
extern SimChannel kSimChannels[SIM_CH_COUNT];


// EOF sim_config.h
