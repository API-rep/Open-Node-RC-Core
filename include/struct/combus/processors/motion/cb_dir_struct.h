/******************************************************************************
 * @file  cb_dir_struct.h
 * @brief Config struct for cb_dir_fn — direction detection thresholds.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include "core/system/combus/combus_res.h"   // CbusNeutral


// =============================================================================
// 1. CONFIG STRUCT
// =============================================================================

/**
 * @brief Configuration for `cb_dir_fn` — direction detection thresholds.
 *
 * @details Reads a post-ramp bipolaire value, determines direction vs. cfg->neutral,
 *   encodes via DriveStateBus::encode() and writes to proc->outValue.
 *   cfg = &CbDirCfg, state = nullptr.
 */
struct CbDirCfg {
    uint16_t neutral = CbusNeutral;  ///< Standing threshold (default = CbusNeutral = 32767).
};

// EOF cb_dir_struct.h
