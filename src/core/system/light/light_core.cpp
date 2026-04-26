/******************************************************************************
 * @file light_core.cpp
 * @brief Light module — LightCfg parser → statusLED / FastLED driver.
 *
 * @details Iterates the per-channel LightCfg array and dispatches each
 *   entry to a typed handler (PLAIN_PWM, INDICATOR, BEACON, REVERSING, TAIL,
 *   HIGHBEAM).  Replaces the legacy led() switch-case FSM.
 *
 *   Coupling notes (transitional — addressed at step 10 main.cpp cleanup):
 *   - Includes sound_module/state/led_state.h for soundLeds[] and statusLED externs.
 *   - Writes gEngineSimState.indicatorOn so the audio task triggers the
 *     indicator click sound (engine_sim_state.h).
 *****************************************************************************/

#ifdef LIGHT_ENABLE

#include "light_core.h"

#include <Arduino.h>
#include <statusLED.h>
#include <core/system/light/light.h>          // light_leds_array()

  // gEngineSimState — indicator click feed to audio task
#include <sound_module/system/sound/engine_sim_state.h>

#if defined NEOPIXEL_ENABLED
#  include <FastLED.h>
#endif


// =============================================================================
// 1. PRIVATE HANDLERS
// =============================================================================

/**
 * @brief PLAIN_PWM handler — mask-driven brightness with optional xenon overlay.
 *
 * @details Priority (early-exit):
 *   1. activeMask & runtimeMask non-zero → brightness (dipDim-reduced if applyDipDim, xenon-boosted).
 *   2. parkBrightness > 0 AND PARKING_ON set → parkBrightness (no separate parkMask).
 *   3. otherwise → off; xenon timer reset.
 */
static void drivePlainChannel(statusLED& led, const LightCfg& d,
                               uint16_t runtimeMask, uint16_t runLevelMask, uint8_t crankingDim,
                               uint8_t dipDim) {

  // Wired-OR activation: channel is active if any bit in activeMask is present in runLevelMask OR runtimeMask
  const bool active     = (d.activeMask & runtimeMask) != 0u || (d.activeMask & runLevelMask) != 0u;
  const bool park       = !active && (d.parkBrightness > 0u) && ((runtimeMask & LightBit::PARKING_ON) != 0u);
  const bool lightIsOff = !active && !park;

    // --- Xenon overlay ---
  static uint32_t xenonMillis = 0;
  static uint8_t  xenonBoost  = 0;
  if (d.xenon) {
    if (lightIsOff) { xenonMillis = millis(); xenonBoost = 0; }
    else { xenonBoost = (millis() - xenonMillis < d.xenon->flashMs) ? d.xenon->flashBoost : 0u; }
  }

    // --- Drive ---
  if (active) {
    const int raw = d.applyDipDim
      ? constrain((int)d.brightness - dipDim + xenonBoost - crankingDim, (int)d.dimFloor, 255)
      : constrain((int)d.brightness + xenonBoost - crankingDim,          (int)d.dimFloor, 255);
    led.pwm((uint8_t)raw);
  } else if (park) {
    led.pwm((uint8_t)constrain((int)d.parkBrightness - crankingDim, (int)d.dimFloor, 255));
  } else {
    led.off();
  }
}


/**
 * @brief INDICATOR handler — flash when activeMask & runtimeMask; sideMarker dim when PARKING_ON.
 */
static void driveIndicatorChannel(statusLED& led, const IndicatorCfg& ic,
                                   const LightCfg& d, uint16_t runtimeMask, uint16_t runLevelMask,
                                   uint8_t crankingDim) {
  const int  fade     = ic.ledType ? 0 : 300;
  // Wired-OR activation for indicator
  const bool chActive = (d.activeMask & runtimeMask) != 0u || (d.activeMask & runLevelMask) != 0u;
  const bool sideOn   = ic.sideMarker && ((runtimeMask & LightBit::PARKING_ON) != 0u);
  const int  offBright = sideOn
    ? constrain((int)d.parkBrightness - crankingDim / 2, 0, 255) : 0;

  if (chActive) {
    led.flash((int)ic.onMs, (int)ic.offMs, 0, 0, 0, fade, offBright);
  } else if (sideOn) {
    led.pwm(offBright);
  } else {
    led.off(fade);
  }
}


/**
 * @brief BEACON handler — flash when activeMask & runtimeMask (ROOF bit).
 */
static void driveBeaconChannel(statusLED& led, const BeaconCfg& bc,
                                const LightCfg& d, uint16_t runtimeMask, uint16_t runLevelMask) {
  // Wired-OR activation for beacon
  if ((d.activeMask & runtimeMask) != 0u || (d.activeMask & runLevelMask) != 0u)
    led.flash((int)bc.onMs, (int)bc.offMs, (int)bc.pauseMs,
              (int)bc.pulses, (int)bc.phaseOffsetMs);
  else
    led.off();
}


/**
 * @brief TAIL handler — mask-driven PWM + full brightness when BRAKING bit set.
 *   Park mode (dim) when PARKING_ON set and parkBrightness > 0.
 */
static void driveTailChannel(statusLED& led, const LightCfg& d,
                              uint16_t runtimeMask, uint16_t runLevelMask, uint8_t crankingDim) {
  // Wired-OR activation for tail
  const bool chActive = (d.activeMask & runtimeMask) != 0u || (d.activeMask & runLevelMask) != 0u;
  const bool chPark   = !chActive && (d.parkBrightness > 0u)
                        && ((runtimeMask & LightBit::PARKING_ON) != 0u);
  if ((runtimeMask & LightBit::BRAKING) != 0u) {
    led.pwm((uint8_t)constrain(255 - crankingDim, 0, 255));
  } else if (chActive) {
    led.pwm((uint8_t)constrain((int)d.brightness - crankingDim / 2, (int)d.dimFloor, 255));
  } else if (chPark) {
    led.pwm((uint8_t)constrain((int)d.parkBrightness - crankingDim / 2, (int)d.dimFloor, 255));
  } else {
    led.off();
  }
}

// =============================================================================
// 2. IMPLEMENTATION — channel parser loop
// =============================================================================

void light_core_update(const LightState& state,
                       const LightCfg* descriptors, uint8_t count,
                       const LightModuleCfg& mod) {

    // --- 0. Null-guard: SLEEPING / RESET — hard-off all channels immediately ---
  if (state.runLevelMask == 0u) {
    statusLED* const arr = light_leds_array();
    if (arr)
      for (uint8_t i = 0; i < count; ++i)
        arr[descriptors[i].lightChannel].off();
    return;
  }

    // --- 1. Cranking dimming ---
  static uint32_t flickerMillis = 0;
  static uint8_t  crankingDim   = 0;

  const bool cranking = (state.runtimeMask & LightBit::CRANKING) != 0u;
  if (mod.flickeringWhileCranking) {
    if (millis() - flickerMillis > 30u) {
      flickerMillis = millis();
      crankingDim = cranking ? (uint8_t)random(25, 55) : 0u;
    }
  } else {
    crankingDim = cranking ? 50u : 0u;
  }

    // --- 2. Activation gate: wired-OR model ---
  // For dipDim and indicatorOn, use the union of runLevelMask and runtimeMask
  const LightBitmask rm_or = state.runLevelMask | state.runtimeMask;
  const uint8_t dipDim = (rm_or & (LightBit::FLASHER | LightBit::HIGH_BEAM)) ? 10u : 170u;

    // --- 3. Feed indicator state to audio task (indicator click sound) ---
  gEngineSimState.indicatorOn = (rm_or & (LightBit::IND_L | LightBit::IND_R)) != 0u;

    // --- 4. Channel parser loop ---
  statusLED* const arr = light_leds_array();
  if (!arr) return;
  for (uint8_t i = 0; i < count; ++i) {
    const LightCfg& d = descriptors[i];
    statusLED& led = arr[d.lightChannel];

    switch (d.type) {

    case LedChannelType::PLAIN_PWM:
      drivePlainChannel(led, d, state.runtimeMask, state.runLevelMask, crankingDim, dipDim);
      break;

    case LedChannelType::INDICATOR:
      driveIndicatorChannel(led, *d.indicator, d, state.runtimeMask, state.runLevelMask, crankingDim);
      break;

    case LedChannelType::BEACON:
      driveBeaconChannel(led, *d.beacon, d, state.runtimeMask, state.runLevelMask);
      break;

    case LedChannelType::TAIL:
      driveTailChannel(led, d, state.runtimeMask, state.runLevelMask, crankingDim);
      break;
    }
  }
}


// =============================================================================
// 3. IMPLEMENTATION — neopixel bar
// =============================================================================

void light_core_update_neopixel(const LightState& state, const LightModuleCfg& mod) {
#if defined NEOPIXEL_ENABLED

  if (!mod.neopixel || mod.neopixel->count == 0 || state.runLevelMask == 0u) return;
  const NeopixelCfg& np = *mod.neopixel;

  // Wired-OR for neopixel overlays
  const LightBitmask rm = state.runLevelMask | state.runtimeMask;
  static uint32_t lastNeopixelTime   = millis();
  static bool     knightRiderLatch   = false;
  static bool     unionJackLatch     = false;
  static bool     neopixelShow       = false;

    // --- High-beam overlay (all white when high-beam or flasher) ---
  if (np.asHighBeam) {
    static uint32_t lastHighbeamTime = millis();
    if (millis() - lastHighbeamTime > 20u) {
      lastHighbeamTime = millis();
      if (!knightRiderLatch && (rm & LightBit::ROOF) == 0u && !unionJackLatch) {
        if ((rm & (LightBit::HIGH_BEAM | LightBit::FLASHER)) != 0u)
          fill_solid(neoPixelLEDs, np.count, CRGB::White);
        else
          fill_solid(neoPixelLEDs, np.count, CRGB::Black);
      }
      neopixelShow = true;
    }
  }

  switch (np.mode) {

  case 1: {  // Demo (reserved — static hue, pulseWidth channel unavailable) ---
    if (millis() - lastNeopixelTime > 20u) {
      lastNeopixelTime = millis();
      constexpr uint8_t kDemoHue = 128u;
      neoPixelLEDs[0] = CHSV(kDemoHue, 255, 255);
      if (np.count > 1) neoPixelLEDs[1] = CRGB::Red;
      if (np.count > 2) neoPixelLEDs[2] = CRGB::White;
      if (np.count > 3) neoPixelLEDs[3] = CRGB::Yellow;
      if (np.count > 4) neoPixelLEDs[4] = CRGB::Blue;
      if (np.count > 5) neoPixelLEDs[5] = CRGB::Green;
      neopixelShow = true;
    }
    break;
  }

  case 2: {  // Knight Rider scanner -----------------------------------------
    static int16_t increment = 1;
    static int16_t counter   = 0;

    if (millis() - lastNeopixelTime > 91u) {
      lastNeopixelTime = millis();
      if ((rm & LightBit::ROOF) != 0u || knightRiderLatch) {
        if (counter >= (int16_t)np.count - 1) increment = -1;
        if (counter <= 0)                               increment =  1;
        knightRiderLatch    = (counter > 0);
        neoPixelLEDs[counter]    = CRGB::Red;
        counter            += increment;
      } else {
        counter = 0;
      }
      for (uint8_t i = 0; i < np.count; ++i)
        neoPixelLEDs[i].nscale8(160);
      neopixelShow = true;
    }
    break;
  }

  case 3: {  // Bluelight double-flash ----------------------------------------
    static uint32_t lastBluelightTime = millis();

    if (millis() - lastNeopixelTime > 20u) {
      lastNeopixelTime = millis();
      if ((rm & LightBit::ROOF) != 0u) {
        const uint32_t elapsed = millis() - lastBluelightTime;
        if (elapsed < 160u) {
          for (uint8_t i = 0; i < np.count; i += 2) {
            neoPixelLEDs[i    ] = CRGB::Red;
            if (i + 1 < np.count) neoPixelLEDs[i + 1] = CRGB::Blue;
          }
        } else if (elapsed < 300u) {
          fill_solid(neoPixelLEDs, np.count, CRGB::Black);
        } else if (elapsed < 460u) {
          for (uint8_t i = 0; i < np.count; i += 2) {
            neoPixelLEDs[i    ] = CRGB::Blue;
            if (i + 1 < np.count) neoPixelLEDs[i + 1] = CRGB::Red;
          }
        } else if (elapsed < 600u) {
          fill_solid(neoPixelLEDs, np.count, CRGB::Black);
        } else {
          lastBluelightTime = millis();
        }
      } else {
        fill_solid(neoPixelLEDs, np.count, CRGB::Black);
      }
      neopixelShow = true;
    }
    break;
  }

  case 4: {  // Union Jack animation ------------------------------------------
    static uint32_t lastUnionJackTime = millis();
    static uint8_t  animStep          = 1;

    if ((rm & LightBit::ROOF) != 0u || unionJackLatch) {
      unionJackLatch = true;
      if (millis() - lastUnionJackTime > 789u) {
        lastUnionJackTime = millis();
        switch (animStep) {
          case 1: case 9:
            if (np.count >= 8) {
              neoPixelLEDs[0]=CRGB::Red;  neoPixelLEDs[1]=CRGB::Blue;
              neoPixelLEDs[2]=CRGB::Blue; neoPixelLEDs[3]=CRGB::Red;
              neoPixelLEDs[4]=CRGB::Red;  neoPixelLEDs[5]=CRGB::Blue;
              neoPixelLEDs[6]=CRGB::Blue; neoPixelLEDs[7]=CRGB::Red;
            } break;
          case 2: case 8:
            if (np.count >= 8) {
              neoPixelLEDs[0]=CRGB::White; neoPixelLEDs[1]=CRGB::Red;
              neoPixelLEDs[2]=CRGB::Blue;  neoPixelLEDs[3]=CRGB::Red;
              neoPixelLEDs[4]=CRGB::Red;   neoPixelLEDs[5]=CRGB::Blue;
              neoPixelLEDs[6]=CRGB::Red;   neoPixelLEDs[7]=CRGB::White;
            } break;
          case 3: case 7:
            if (np.count >= 8) {
              neoPixelLEDs[0]=CRGB::Blue;  neoPixelLEDs[1]=CRGB::White;
              neoPixelLEDs[2]=CRGB::Red;   neoPixelLEDs[3]=CRGB::Red;
              neoPixelLEDs[4]=CRGB::Red;   neoPixelLEDs[5]=CRGB::Red;
              neoPixelLEDs[6]=CRGB::White; neoPixelLEDs[7]=CRGB::Blue;
            } break;
          case 4: case 5: case 6:
            fill_solid(neoPixelLEDs, np.count, CRGB::Red); break;
          case 10:
            fill_solid(neoPixelLEDs, np.count, CRGB::Black);
            animStep       = 0;
            unionJackLatch = false;
            break;
          default: break;
        }
        animStep++;
        neopixelShow = true;
      }
    } else {
      animStep          = 1;
      lastUnionJackTime = millis();
    }
    break;
  }

  case 5: {  // B33lz3bub Austria cab light -----------------------------------
    if (millis() - lastNeopixelTime > 20u) {
      lastNeopixelTime = millis();
        // Hue from potentiometer unavailable in ComBus model — use fixed hue
      constexpr uint8_t kCabHue = 128u;
      for (uint8_t i = 0; i < np.count; ++i)
        neoPixelLEDs[i] = CHSV(kCabHue, 255, 255);
      neopixelShow = true;
    }
    break;
  }

  default:
    break;
  }

    // --- Commit pixel buffer (rate-limited to 20 ms) ---
  static uint32_t lastRefreshTime = millis();
  if (millis() - lastRefreshTime > 20u && neopixelShow) {
    lastRefreshTime = millis();
    FastLED.show();
    neopixelShow = false;
  }

#endif  // NEOPIXEL_ENABLED
}

#endif  // LIGHT_ENABLE

// EOF light_core.cpp
