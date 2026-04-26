/******************************************************************************
 * @file neopixel.h
 * @brief WS2812 NeoPixel strip — hardware init and pixel buffer.
 *
 * @details Provides the FastLED strip initialisation (neopixel_init()) and
 *   the global pixel buffer (neoPixelLEDs[]).
 *
 *   Hardware parameters (NeoPixelPin, NeoPixelCount, NeoPixelBrightness,
 *   NeoPixelMaxMilliamps) are constexpr values that MUST be declared by
 *   the active board header (sound_board_*.h) before this header is included.
 *   A static_assert below catches any board that forgets to define them.
 *
 *   Call order:
 *     neopixel_init() once from hw_init_light(), after pin registry is up.
 *****************************************************************************/
#pragma once

#ifdef NEOPIXEL_ENABLED

#include <FastLED.h>
#include <stdint.h>


// =============================================================================
// 1. BOARD CONTRACT CHECK
// =============================================================================
// These constexpr must be provided by the board header.  This fires at compile
// time if a new board defines NEOPIXEL_ENABLED but forgets the parameters.
static_assert(NeoPixelCount      >  0,   "Board must define constexpr NeoPixelCount");
static_assert(NeoPixelPin        <  40,  "Board must define constexpr NeoPixelPin (valid ESP32 GPIO)");
static_assert(NeoPixelMaxMilliamps > 0,  "Board must define constexpr NeoPixelMaxMilliamps");


// =============================================================================
// 2. PIXEL BUFFER (owned by neopixel.cpp)
// =============================================================================

/// FastLED WS2812 pixel buffer — size = NeoPixelCount.
extern CRGB neoPixelLEDs[];


// =============================================================================
// 3. INIT API
// =============================================================================

/**
 * @brief  Initialise the WS2812 strip from board-provided constexpr parameters.
 *
 * @details Calls FastLED.addLeds, setCorrection, setBrightness,
 *          setMaxPowerInVoltsAndMilliamps.  Safe to call only once
 *          from hw_init_light().
 */
void neopixel_init();

#endif  // NEOPIXEL_ENABLED

// EOF neopixel.h
