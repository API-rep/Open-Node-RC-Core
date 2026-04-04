/******************************************************************************
 * @file combus_init.cpp
 * @brief ComBus core initialisation — implementation.
 *****************************************************************************/

#include "combus_init.h"

#include <core/system/debug/debug.h>


// =============================================================================
// 1. COMBUS INIT
// =============================================================================

void combus_init(uint8_t nAnalog, uint8_t nDigital)
{
    sys_log_info("[COMBUS] ComBus ready — analog:%u digital:%u channels.\n",
                 nAnalog, nDigital);
}

// EOF combus_init.cpp
