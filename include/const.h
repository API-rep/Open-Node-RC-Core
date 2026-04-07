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

	/// ADC full-scale count — derived from MCU resolution at compile time.
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
 * @brief Working resolution type for all ComBus channel values.
 *
 * @details Unsigned integer type used throughout the motion processing chain
 *   and ComBus read/write operations.  Changing this alias propagates to
 *   every module that processes ComBus data without touching individual files.
 *
 *   Supported widths and their characteristics:
 *   - `uint8_t`  — lite mode   (256 levels, minimal RAM and bandwidth)
 *   - `uint16_t` — standard    (65 536 levels — default, matches hardware ComBus)
 *   - `uint32_t` — extended    (future: high-resolution encoders)
 *
 *   Standard uint16_t mapping used throughout the project:
 *   - `0`      → full reverse / minimum  (≈ 1000 µs servo pulse)
 *   - `32767`  → neutral / center        (≈ 1500 µs servo pulse)
 *   - `65535`  → full forward / maximum  (≈ 2000 µs servo pulse)
 */
using combus_t = uint16_t;

// EOF const.h
