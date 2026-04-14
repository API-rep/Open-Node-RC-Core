/*!****************************************************************************
 * @file  config.h
 * @brief Core Environement top configuration data
 * This section contain all the configuration data used to setup the RC core environement.
 * These data are shared by all "nodes" (remote, machine ...) to ensure compatibility and coherence.
 * Modififacations will affect all sub projects. Edit it carefully.
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include "machines/combus_types.h"
#include "inputs/inputs.h"
#include "outputs/outputs.h"

/**
* Environement variable
*/

static constexpr uint32_t ComBusDisconnectTimeoutMs = 500u;  // isDrived cleared after this gap without a valid frame
static constexpr uint32_t DebounceMs               = 100u;  // digital input debounce delay


// EOF config.h