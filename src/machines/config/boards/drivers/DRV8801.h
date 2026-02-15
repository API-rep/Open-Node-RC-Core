/*!****************************************************************************
 * @file  DRV8801.h
 * @brief DRV8801 DC driver module configuration
 * This board embeed one H-Bridge with:
 * - Enable/phase or dir/PWM input
 * - Sleep input (disable by default)
 * - decay mode
 *     -> HIGH = slow decay (default)
 *     -> LOW = Fast-decay synchronous rectification
 * - Fault output
 *******************************************************************************/// 
#pragma once

#include <pin_defs.h>

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

// =============================================================================
// 1. DRIVER CONFIGURATION
// =============================================================================

inline constexpr DriverModel DRV8801 {
	.infoName = "DRV8801",  
	
	  // Default electrical levels and modes
	.sleepActiveLevel  = ActiveLevel::ActiveLow,
  .DecayPinHighState = DecayMode::SlowDecay,
	.defaultdDecayMode = DecayMode::Unset,
	.faultMode         = PinMode::OutputOpenDrain
};

// EOF DRV8801.h