/******************************************************************************
 * @file  cb_center_struct.h
 * @brief Config struct for cb_center_fn — neutral reference for signed deviation.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include "core/system/combus/combus_res.h"   // CbusNeutral


// =============================================================================
// 1. CONFIG STRUCT
// =============================================================================

/**
 * @brief Configuration for `cb_center_fn` — neutral reference for signed deviation.
 *
 * @details `value = (uint16_t)(int16_t)(value − cfg->neutral)`.
 *   Defaults to CbusNeutral = 32767 (standard ComBus bipolar center).
 *   cfg = &CbCenterCfg, state = nullptr.
 */
struct CbCenterCfg {
    uint16_t neutral = CbusNeutral;  ///< Center reference (default = CbusNeutral = 32767).
};

// EOF cb_center_struct.h
