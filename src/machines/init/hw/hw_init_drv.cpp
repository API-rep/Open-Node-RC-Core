/*****************************************************************************
 * @file hw_init_drv.cpp
 * @brief DC driver hardware initialization — machine environment wrapper.
 *****************************************************************************/

#include "hw_init_drv.h"
#include "../sys/sys_init.h"    // pinReg

// =============================================================================
// 1. INIT
// =============================================================================

void dcDriverInit(const EnvCfg& config)
{
  dcDriverInit(config, pinReg);
}

// EOF hw_init_drv.cpp