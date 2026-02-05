/*****************************************************************************
 * @file drv_control.h
 * @brief DC drivers batch control and state management
 * 
 * This module provides high-level routines to manage DC driver states 
 * (Sleep, Wakeup, Stop) across all configured devices.
 *****************************************************************************/
#pragma once

#include <core/config/combus/combus.h>

// =============================================================================
// 1. DRIVER STATE MANAGEMENT
// =============================================================================

/**
 * @brief Put all DC drivers into low-power sleep mode
 */
void sleepAllDcDrivers(const Machine &config);

/**
 * @brief Wake up all DC drivers from sleep mode
 */
void wakeupAllDcDrivers(const Machine &config);

/**
 * @brief Enable all DC drivers output bridges
 */
void enableAllDcDrivers(const Machine &config);

/**
 * @brief Disable all DC drivers output bridges
 */
void disableAllDcDrivers(const Machine &config);

/**
 * @brief Stop all DC drivers outputs
 */
void stopAllDcDrivers(const Machine &config);

// EOF drv_control.h
