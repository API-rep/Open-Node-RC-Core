/*****************************************************************************
 * @file hw_init_srv.h
 * @brief Servo hardware initialization — machine environment wrapper.
 *****************************************************************************/
#pragma once

#include <core/system/hw/dev/srv_dev.h>


// =============================================================================
// 1. INIT  (zero-arg environment entry point)
// =============================================================================

/// Initialize all active servo output channels for the machine environment.
/// @details Calls servoInit(config, pinReg) with the machine EnvCfg.
void servoInit(const EnvCfg& config);

// EOF hw_init_srv.h
