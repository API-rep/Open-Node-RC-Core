/******************************************************************************
 * @file combus.cpp
 * @brief ComBus core initialisation — implementation.
 *****************************************************************************/

#include "combus.h"
#include "combus_access.h"

#include <core/system/debug/debug.h>


// =============================================================================
// 1. COMBUS INIT
// =============================================================================

void combus_init(uint8_t nAnalog, uint8_t nDigital, uint8_t nodeGroup)
{
    combus_set_node_group(nodeGroup);
    sys_log_info("[COMBUS] ComBus ready — group:0x%02X  analog:%u  digital:%u channels.\n",
                 nodeGroup, nAnalog, nDigital);
}

// EOF combus.cpp
