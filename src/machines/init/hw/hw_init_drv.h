/*****************************************************************************
 * @file hw_init_drv.h
 * @brief DC driver hardware initialization — machine environment wrapper.
 *****************************************************************************/
#pragma once

#include <core/system/hw/dev/drv_dev.h>


// =============================================================================
// 1. INIT  (single-arg environment entry point)
// =============================================================================

/// Initialize all active DC motor output channels for the machine environment.
/// @details Calls dcDriverInit(config, pinReg) with the machine EnvCfg.
void dcDriverInit(const EnvCfg& config);

// EOF hw_init_drv.h
