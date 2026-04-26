/******************************************************************************
 * @file neopixel.cpp
 * @brief WS2812 NeoPixel strip — pixel buffer definition and init.
 *
 * @details Owns the FastLED pixel buffer and implements neopixel_init().
 *
 *   All hardware parameters (pin, count, brightness, power cap) are
 *   resolved from constexpr constants declared in sound_module/config/config.h
 *   (NeoPixelPin, NeoPixelCount, NeoPixelBrightness, NeoPixelMaxMilliamps).
 *   Board headers may override the defaults before config.h is included.
 *
 *   Include path requirement: sound_module/config/config.h must be on the
 *   include path (provided by the sound_node_volvo platformio env via
 *   -I src/sound_module).
 *****************************************************************************/

#include "sound_module/config/config.h"   // NeoPixelPin, NeoPixelCount, NEOPIXEL_ENABLED
#include "neopixel.h"

#ifdef NEOPIXEL_ENABLED


// =============================================================================
// 1. PIXEL BUFFER DEFINITION
// =============================================================================

CRGB neoPixelLEDs[NeoPixelCount];  ///< FastLED WS2812 pixel buffer.


// =============================================================================
// 2. INIT
// =============================================================================

void neopixel_init()
{
    FastLED.addLeds<NEOPIXEL, NeoPixelPin>(neoPixelLEDs, NeoPixelCount);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(NeoPixelBrightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, NeoPixelMaxMilliamps);
}

#endif  // NEOPIXEL_ENABLED

// EOF neopixel.cpp
