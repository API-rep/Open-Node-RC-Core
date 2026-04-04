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


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Log ComBus channel counts at boot (sanity trace only).
 *
 * @param nAnalog   Number of analog ComBus channels for this build.
 * @param nDigital  Number of digital ComBus channels for this build.
 */
void combus_init(uint8_t nAnalog, uint8_t nDigital);

// EOF combus.h
