/*****************************************************************************
 * @file hw_init.h
 * @brief Hardware configuration and device initialization
 *****************************************************************************/
#pragma once

#include <core/config/combus/combus_types.h>
#include <core/utils/debug/debug.h>

#include "hw_init_drv.h"
#include "hw_init_srv.h"


// =============================================================================
// 1. MAIN INITIALIZATION ROUTINE
// =============================================================================

	/// Hardware initialization — config check then driver and servo setup.
void hw_init();

// EOF hw_init.h