/******************************************************************************
 * @file neopixel_modes.cpp
 * @brief Sound module — WS2812 NeoPixel animation modes.
 *
 * @details Implements the five legacy neopixelMode animations driven from the
 *   main-loop path (NEOPIXEL_ENABLED, LIGHT_ENABLE absent).
 *
 *   Globals accessed via extern (all defined in the main.cpp TU):
 *     neoPixelLEDs[], neopixelMode                        — FastLED pixel buffer + mode selector
 *     sirenTrigger, blueLightTrigger                 — sound/light event triggers
 *     headLightsHighBeamOn, headLightsFlasherOn      — light state flags
 *     pulseWidth[]                                   — RC channel pulse widths
 *
 *   NeoPixelCount and NEOPIXEL_HIGHBEAM are defined in sound_board_esp32.h
 *   and propagated through config/config.h (boards/boards.h).
 ******************************************************************************/

#include "neopixel_modes.h"

#include <Arduino.h>
#include "sound_module/config/config.h"  // NEOPIXEL_ENABLED, NeoPixelCount, NEOPIXEL_HIGHBEAM

#if defined NEOPIXEL_ENABLED
#include <FastLED.h>
#include <core/system/light/neopixel/neopixel.h>  // extern CRGB neoPixelLEDs[]


// =============================================================================
// 1. EXTERN DECLARATIONS  (rc_engine_sound globals from main.cpp TU)
// =============================================================================

// neoPixelLEDs[] declared via neopixel.h above.
extern uint8_t          neopixelMode;           ///< Active animation (1–5) — from 6_Lights.h.

extern volatile boolean sirenTrigger;
extern volatile boolean blueLightTrigger;
extern volatile boolean headLightsHighBeamOn;
extern volatile boolean headLightsFlasherOn;
extern uint16_t         pulseWidth[];


// =============================================================================
// 2. NEOPIXEL MODE DISPATCHER
// =============================================================================

void updateRGBLEDs()
{
  static uint32_t lastNeopixelTime = millis();
  static bool knightRiderLatch = false;
  static bool unionJackLatch = false;
  static bool neopixelShow = false;

#ifdef NEOPIXEL_HIGHBEAM // Neopixel bar is used as high beam as well --------------------
  static uint32_t lastNeopixelHighbeamTime = millis();
  if (millis() - lastNeopixelHighbeamTime > 20)
  { // Every 20 ms
    lastNeopixelHighbeamTime = millis();

    if (!knightRiderLatch && !sirenTrigger && !blueLightTrigger && !unionJackLatch)
    {
      if (headLightsHighBeamOn || headLightsFlasherOn)
        fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::White);
      else
        fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::Black);
    }
    neopixelShow = true;
  }
#endif

  switch (neopixelMode)
  {

  case 1: // Demo -------------------------------------------------------------
    if (millis() - lastNeopixelTime > 20)
    { // Every 20 ms
      lastNeopixelTime = millis();

      uint8_t hue = map(pulseWidth[1], 1000, 2000, 0, 255);

      neoPixelLEDs[0] = CHSV(hue, hue < 255 ? 255 : 0, hue > 0 ? 255 : 0);
      neoPixelLEDs[1] = CRGB::Red;
      neoPixelLEDs[2] = CRGB::White;
      neoPixelLEDs[3] = CRGB::Yellow;
      neoPixelLEDs[4] = CRGB::Blue;
      neoPixelLEDs[5] = CRGB::Green;
      neopixelShow = true;
    }
    break;

  case 2: // Knight Rider scanner -------------------------------------
    static int16_t increment = 1;
    static int16_t counter = 0;

    if (millis() - lastNeopixelTime > 91)
    { // Every 91 ms (must match with sound)
      lastNeopixelTime = millis();

      if (sirenTrigger || knightRiderLatch)
      { // Only active, if siren signal!
        if (counter >= NeoPixelCount - 1)
          increment = -1;
        if (counter <= 0)
          increment = 1;
        knightRiderLatch = (counter > 0);
        neoPixelLEDs[counter] = CRGB::Red;
        counter += increment;
      }
      else
      {
        counter = 0;
      }
      for (int i = 0; i < NeoPixelCount; i++)
      {
        neoPixelLEDs[i].nscale8(160); // 160
      }
      neopixelShow = true;
    }
    break;

  case 3: // Bluelight ----------------------------------------------------
    static uint32_t lastNeopixelBluelightTime = millis();

    if (millis() - lastNeopixelTime > 20)
    { // Every 20 ms
      lastNeopixelTime = millis();
      if (blueLightTrigger)
      {
        if (millis() - lastNeopixelBluelightTime > 0)
        { // Step 1
          neoPixelLEDs[0] = CRGB::Red;
          neoPixelLEDs[1] = CRGB::Blue;
          neoPixelLEDs[3] = CRGB::Red;
          neoPixelLEDs[4] = CRGB::Blue;
          neoPixelLEDs[6] = CRGB::Red;
          neoPixelLEDs[7] = CRGB::Blue;
        }
        if (millis() - lastNeopixelBluelightTime > 160)
        { // Step 2
          fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::Black);
        }
        if (millis() - lastNeopixelBluelightTime > 300)
        { // Step 3
          neoPixelLEDs[0] = CRGB::Blue;
          neoPixelLEDs[1] = CRGB::Red;
          neoPixelLEDs[3] = CRGB::Blue;
          neoPixelLEDs[4] = CRGB::Red;
          neoPixelLEDs[6] = CRGB::Blue;
          neoPixelLEDs[7] = CRGB::Red;
        }
        if (millis() - lastNeopixelBluelightTime > 460)
        { // Step 4
          fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::Black);
        }
        if (millis() - lastNeopixelBluelightTime > 600)
        { // Step 5
          lastNeopixelBluelightTime = millis();
        }
      }
      else
        fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::Black); // Off
      neopixelShow = true;
    }
    break;

  case 4: // United Kingdom animation ---------------------------------------
    static uint32_t lastNeopixelUnionJackTime = millis();
    static uint8_t animationStep = 1;

    if (sirenTrigger || unionJackLatch)
    {
      unionJackLatch = true;
      if (millis() - lastNeopixelUnionJackTime > 789)
      { // Every 789 ms (must match with sound "BritishNationalAnthemSiren.h")
        lastNeopixelUnionJackTime = millis();
        if (animationStep == 1 || animationStep == 9)
        { // Step 1 or 9
          neoPixelLEDs[0] = CRGB::Red;
          neoPixelLEDs[1] = CRGB::Blue;
          neoPixelLEDs[2] = CRGB::Blue;
          neoPixelLEDs[3] = CRGB::Red;
          neoPixelLEDs[4] = CRGB::Red;
          neoPixelLEDs[5] = CRGB::Blue;
          neoPixelLEDs[6] = CRGB::Blue;
          neoPixelLEDs[7] = CRGB::Red;
        }
        if (animationStep == 2 || animationStep == 8)
        { // Step 2 or 8
          neoPixelLEDs[0] = CRGB::White;
          neoPixelLEDs[1] = CRGB::Red;
          neoPixelLEDs[2] = CRGB::Blue;
          neoPixelLEDs[3] = CRGB::Red;
          neoPixelLEDs[4] = CRGB::Red;
          neoPixelLEDs[5] = CRGB::Blue;
          neoPixelLEDs[6] = CRGB::Red;
          neoPixelLEDs[7] = CRGB::White;
        }
        if (animationStep == 3 || animationStep == 7)
        { // Step 3 or 7
          neoPixelLEDs[0] = CRGB::Blue;
          neoPixelLEDs[1] = CRGB::White;
          neoPixelLEDs[2] = CRGB::Red;
          neoPixelLEDs[3] = CRGB::Red;
          neoPixelLEDs[4] = CRGB::Red;
          neoPixelLEDs[5] = CRGB::Red;
          neoPixelLEDs[6] = CRGB::White;
          neoPixelLEDs[7] = CRGB::Blue;
        }
        if (animationStep == 4 || animationStep == 5 || animationStep == 6)
        { // Step 4
          neoPixelLEDs[0] = CRGB::Red;
          neoPixelLEDs[1] = CRGB::Red;
          neoPixelLEDs[2] = CRGB::Red;
          neoPixelLEDs[3] = CRGB::Red;
          neoPixelLEDs[4] = CRGB::Red;
          neoPixelLEDs[5] = CRGB::Red;
          neoPixelLEDs[6] = CRGB::Red;
          neoPixelLEDs[7] = CRGB::Red;
        }
        if (animationStep == 10)
        { // Step 10
          fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::Black);
          animationStep = 0;
          unionJackLatch = false;
        }
        animationStep++;
        neopixelShow = true;
      }
    }
    else
    {
      animationStep = 1;
      lastNeopixelUnionJackTime = millis();
    }
    break;

  case 5: // B33lz3bub Austria cab light ---------------------------------
    if (millis() - lastNeopixelTime > 20)
    { // Every 20 ms
      lastNeopixelTime = millis();

      uint8_t hue = map(pulseWidth[7], 1000, 2000, 0, 255); // map pulseWidth[7] from poti VRA to hue
      if (hue <= 20)
      { // LEDs off
        fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::Black);
      }
      else if (hue > 20 && hue < 235)
      { // different colors depending on potentiometer VRA CH7
        for (int i = 0; i < NeoPixelCount; i++)
        {
          neoPixelLEDs[i] = CHSV(hue, hue < 255 ? 255 : 0, hue > 0 ? 255 : 0);
        }
      }
      else if (hue >= 235 && hue < 250)
      { // colors red-white-red -> flag color of Austria ;-)
        neoPixelLEDs[0] = CRGB::Red;
        neoPixelLEDs[1] = CRGB::White;
        neoPixelLEDs[2] = CRGB::Red;
      }
      else
      {
        fill_solid(neoPixelLEDs, NeoPixelCount, CRGB::White); // only white
      }
      neopixelShow = true;
    }
    break;
  } // End of switch case -----

  // Neopixel refresh for all modes above -----------------------------------------------
  static uint32_t lastNeopixelRefreshTime = millis();
  if (millis() - lastNeopixelRefreshTime > 20 && neopixelShow)
  { // Every 20 ms
    lastNeopixelRefreshTime = millis();
    FastLED.show();
    neopixelShow = false;
  }
}

#else // !NEOPIXEL_ENABLED

void updateRGBLEDs() {}  // no-op

#endif // NEOPIXEL_ENABLED


// EOF neopixel_modes.cpp
