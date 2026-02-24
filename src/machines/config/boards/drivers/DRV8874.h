/*!****************************************************************************
 * @file  DRV8874.h
 * @brief DRV8874 DC driver module configuration
 * This board embeed one H-Bridge with:
 * - Enable/phase input
 * - Sleep input (disable by default)
 * - Brake output (always on, freewheel when sleep is active)
 *     -> HIGH = fixed Off-time current regulation, automatic Retry
 *     -> LOW = cycle-by-cycle current regulation, automatic Retry
 * - Fault output
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

/**
 * @brief DC Driver config structure definition
 * Place here all driver configuration contants
 */

inline constexpr DriverModel DRV8874 {
  .infoName = "DRV8874",  
  .sleepActiveLevel  = ActiveLevel::ActiveLow,
  .enableActiveLevel = ActiveLevel::ActiveHigh,
  .DecayPinHighState = DecayMode::Unset,
  .defaultdDecayMode = DecayMode::Unset,
  .faultMode         = PinMode::OutputOpenDrain
};

// EOF DRV8801.h