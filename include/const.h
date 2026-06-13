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

	/// ADC full-scale size — derived from MCU resolution at compile time.
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  static constexpr uint16_t ADC_MAX_COUNT = 4095;   // 12-bit ADC
#elif defined(__AVR__)
  static constexpr uint16_t ADC_MAX_COUNT = 1023;   // 10-bit ADC
#else
  #error "ADC_MAX_COUNT: unknown CPU — define ADC resolution in const.h for this target."
#endif



// =============================================================================
// COMBUS CHANNEL TYPE
// =============================================================================

/**
 * @brief Combus analog value resolution
 *
 * @details Changing this alias propagates to all modules relying on ComBus
 *   analog channels.
 *
 *   Standard mapping:
 *   - 0      → minimum
 *   - 32767  → neutral
 *   - 65535  → maximum
 */
using combus_t = uint16_t;

// EOF const.h
