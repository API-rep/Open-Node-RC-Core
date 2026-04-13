/*****************************************************************************
 * @file hw_init.h
 * @brief Hardware configuration and device initialization
 *****************************************************************************/
#pragma once

#include <core/config/machines/combus_types.h>
#include <core/system/debug/debug.h>

#include "hw_init_drv.h"
#include "hw_init_srv.h"
#include "hw_init_sig.h"
#include "hw_init_com.h"


// =============================================================================
// 1. MAIN INITIALIZATION ROUTINE
// =============================================================================

	/// Hardware initialization — config check then driver and servo setup.
void hw_init();

// EOF hw_init.h