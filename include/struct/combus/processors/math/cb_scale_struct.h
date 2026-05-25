/******************************************************************************
 * @file  cb_scale_struct.h
 * @brief Config struct for cb_scale_fn — linear domain rescale.
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. CONFIG STRUCT
// =============================================================================

/**
 * @brief Configuration for `cb_scale_fn` — linear domain rescale.
 *
 * @details `value = value × outMax / inMax`.
 *   cfg = &CbScaleCfg, state = nullptr.
 */
struct CbScaleCfg {
    uint16_t inMax;   ///< Input range ceiling (e.g. CbusNeutral after cb_abs_fn).
    uint16_t outMax;  ///< Output range ceiling (e.g. gear[n-1].upShift — init from profile).
};

// EOF cb_scale_struct.h
