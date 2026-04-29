/*****************************************************************************
 * @file hw_init_srv.cpp
 * @brief Servo hardware initialization — machine environment wrapper.
 *****************************************************************************/

#include "hw_init_srv.h"
#include "../sys/sys_init.h"    // pinReg


// =============================================================================
// 1. INIT
// =============================================================================

void servoInit(const EnvCfg& config)
{
	servoInit(config, pinReg);
}

// EOF hw_init_srv.cpp
