/*****************************************************************************
 * @file drv_control.h
 * @brief DC drivers batch control and state management
 * 
 * This module provides high-level routines to manage DC driver states 
 * (Sleep, Wakeup, Stop) across all configured devices.
 *****************************************************************************/
#pragma once

#include <core/config/machines/combus_types.h>

// =============================================================================
// 1. DRIVER STATE MANAGEMENT
// =============================================================================

/**
 * @brief Put all DC drivers into low-power sleep mode
 */
void sleepAllDcDrivers(const EnvCfg &config);

/**
 * @brief Wake up all DC drivers from sleep mode
 */
void wakeupAllDcDrivers(const EnvCfg &config);

/**
 * @brief Enable all DC drivers output bridges
 */
void enableAllDcDrivers(const EnvCfg &config);

/**
 * @brief Disable all DC drivers output bridges
 */
void disableAllDcDrivers(const EnvCfg &config);

/**
 * @brief Stop all DC drivers outputs
 */
void stopAllDcDrivers(const EnvCfg &config);

// EOF drv_control.h
