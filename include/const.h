/*!****************************************************************************
 * @file  const.h
 * @brief Projects constants definition file
 * This file contain the constants definition used to manage the projet
 * These constants enhance main code structure, maintenance and readability
 * Share it with other deveice of the same project (trailer, light module, sound module ...)
 *******************************************************************************/// 
#pragma once

#include <cstdint>
#define PERCENT_MAX           100   // maximum value in %    


// =============================================================================
// ADC SYSTEM CONSTANTS
// =============================================================================

// ADC_REF_VOLTAGE is a board-level constant (voltage regulator output).
// Define AdcRefVoltage in the board config file (e.g. ESP32_8M_6S.h).

	/// ADC full-scale count — derived from MCU resolution at compile time.
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  static constexpr uint16_t ADC_MAX_COUNT = 4095;   // 12-bit ADC
#elif defined(__AVR__)
  static constexpr uint16_t ADC_MAX_COUNT = 1023;   // 10-bit ADC
#else
  #error "ADC_MAX_COUNT: unknown CPU — define ADC resolution in const.h for this target."
#endif

// EOF const.h
