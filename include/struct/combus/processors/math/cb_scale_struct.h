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
 * @details `value = outMin + value × (outMax − outMin) / inMax`.
 *   When `outMin == 0` the formula reduces to the original `value × outMax / inMax`.
 *   cfg = &CbScaleCfg, state = nullptr.
 */
struct CbScaleCfg {
    uint16_t inMax;          ///< Input range ceiling (e.g. CbusNeutral after cb_abs_fn).
    uint16_t outMax;         ///< Output range ceiling (e.g. gear[n-1].upShift — init from profile).
    uint16_t outMin = 0u;   ///< Output floor when input == 0 (e.g. idle RPM). 0 = original behaviour.
                             ///<   For traction: set to gear[0].downShift (idle RPM stored in profile).
};

// EOF cb_scale_struct.h
