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

#include "config/combus.h"

/**
* Environement variable
*/

#define DISCONECT_DELAY         000       // max delay to sleep devices when a controller not respond
#define DEBOUNCE_MS             100       // debounce delay in ms


// EOF config.h