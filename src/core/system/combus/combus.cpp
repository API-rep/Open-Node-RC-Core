/******************************************************************************
 * @file combus.cpp
 * @brief ComBus core initialisation — implementation.
 *****************************************************************************/

#include "combus.h"
#include "combus_access.h"

#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. COMBUS INIT
// =============================================================================

void combus_init(uint8_t nAnalog, uint8_t nDigital)
{
    sys_log_info("[COMBUS] ComBus ready — analog:%u  digital:%u channels.\n",
                 nAnalog, nDigital);
}

// EOF combus.cpp
