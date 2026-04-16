/******************************************************************************
 * @file combus.h
 * @brief ComBus core initialisation — channel count log and sanity check.
 *
 * @details Called from sys_init() (machine env only) after the ComBus config
 *   headers are resolved, before any hardware or transport init.
 *   Logs the analog / digital channel counts so the boot trace reflects the
 *   active ComBus layout for the build.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_struct.h>   // ComBusOwner::GRP_* constants for nodeGroup parameter


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialise ComBus: register node group and log channel counts.
 *
 * @param nAnalog    Number of analog ComBus channels for this build.
 * @param nDigital   Number of digital ComBus channels for this build.
 * @param nodeGroup  Node-group identity of this environment (`ComBusOwner::GRP_*`
 *                   — see `combus_struct.h`). Determines which channels are
 *                   local or not (ex : extension board).
 *                   Pass `EnvNodeGroup` from the env `config.h`.
 */
void combus_init(uint8_t nAnalog, uint8_t nDigital, uint8_t nodeGroup);

// EOF combus.h
