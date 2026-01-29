/*!****************************************************************************
 * @file  DRV8801.h
 * @brief DRV8801 DC driver module configuration
 * This board embeed one H-Bridge with:
 * - Enable/phase or dir/PWM input
 * - Sleep input (disable by default)
 * - Brake output
 *     -> HIGH = slow decay (default)
 *     -> LOW = Fast-decay synchronous rectification
 * - Fault output
 *******************************************************************************/// 
#pragma once

#include <struct.h>
#include <const.h>
#include <macro.h>


/**
 * @brief DC Driver config structure definition
 * Place here all driver configuration contants
 */

inline constexpr DriverModel DRV8801 {
  .infoName = "DRV8801",  
  .sleepPinMode = InputPinMode::ACTIVE_LOW,
  .brakePinMode = InputPinMode::UNDEFINED,
  .faultPinMode = InputPinMode::OPEN_DRAIN_MODE
};

// EOF DRV8801.h