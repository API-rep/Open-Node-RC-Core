/*!****************************************************************************
 * @file  dumper_truck_lights.h
 * @brief Dumper truck light descriptors, indicator/beacon configs, and module config.
 *
 * @details Declares per-channel `LightCfg[]` constexpr array, sub-struct
 *   beacon/indicator instances, and `kLightModuleCfg` for the Dumper Truck
 *   machine template.
 *   Consumed by `light_init()` and `light_core_update()` — never transmitted
 *   over ComBus.
 *
 *   Pin assignments live in the board header (sound_board_esp32_DIYGuy.h).
 *   This file only carries per-vehicle tuning parameters.
 *******************************************************************************///
#pragma once

#include <cstdint>
#include <core/system/light/light_state.h>


// =============================================================================
// 1. CHANNEL ID ENUM
// =============================================================================

/// @brief Logical LED channel indices for kLedDescriptors[].
enum LedCh : uint8_t {
  LED_HEAD    = 0,
  LED_TAIL    = 1,
  LED_IND_L   = 2,
  LED_IND_R   = 3,
  LED_FOG     = 4,
  LED_REVERSE = 5,
  LED_ROOF    = 6,
  LED_SIDE    = 7,
  LED_BEACON1 = 8,
  LED_BEACON2 = 9,
  LED_BRAKE   = 10,
  LED_CAB     = 11,
  LED_CH_COUNT,
};


// =============================================================================
// 2. SUB-STRUCT INSTANCES (sub-struct pointers referenced by descriptors)
// =============================================================================

/// @brief Double-flash blue beacon — gyrophare channel 1 (phase 0 ms).
inline constexpr BeaconCfg kBeaconBlue1    = { 30u, 80u, 400u, 2u,   0u };
/// @brief Double-flash blue beacon — gyrophare channel 2 (phase 330 ms).
inline constexpr BeaconCfg kBeaconBlue2    = { 30u, 80u, 400u, 2u, 330u };

/// @brief Left indicator (non-fading incandescent, standard side).
inline constexpr IndicatorCfg kIndLCfg     = { 375u, 375u, false, false };
/// @brief Right indicator (non-fading incandescent, standard side).
inline constexpr IndicatorCfg kIndRCfg     = { 375u, 375u, false, false };

/// @brief Neopixel bar config for the Dumper Truck.
inline constexpr NeopixelCfg kNeopixelCfg  = {
  .count             =  8u,   ///< WS2812 LED count.
  .brightness        = 127u,  ///< Global FastLED brightness cap (0–255).
  .maxPowerMilliAmps = 100u,  ///< FastLED power budget (mA).
  .mode              =  2u,   ///< 1=Demo 2=KnightRider 3=Bluelight 4=UnionJack 5=B33lz3bub.
  .asHighBeam        = true,  ///< Fill bar white when high-beam or flasher active.
};


// =============================================================================
// 3. RUN-LEVEL MASK TABLE  (layer 2 — permission gate)
// =============================================================================

/**
 * @brief Auto-activation table: maps each RunLevel (IDLE=0 … RESET=5) to the set of
 *   LightBit:: bits that are FORCED ON at that RunLevel, regardless of ComBus.
 *
 * @details With the wired-OR activation model, bits here independently activate channels
 *   (no ComBus command needed).  Only include bits that must auto-on per RunLevel.
 *   ComBus-driven bits (LOW_BEAM, FOG, HIGH_BEAM, IND_L/R, FLASHER, ROOF, REVERSING, BRAKING)
 *   must NOT appear here — they are set in runtimeMask by the interpreter and activate
 *   channels through the OR's second branch.
 *
 *   Null-guard: entries of 0u force ALL channels off regardless of runtimeMask
 *   (enforced by the null-guard in light_core_update).  Used for SLEEPING / RESET.
 *
 *   Dumper Truck:
 *   - IDLE        → PARKING_ON           (engine off: parking lights auto-on)
 *   - STARTING    → PARKING_ON + CAB_ON  (cranking: cab + parking auto-on)
 *   - RUNNING     → CAB_ON               (running: same auto set; all else via ComBus)
 *   - TURNING_OFF → PARKING_ON + CAB_ON  (shutdown: same auto set)
 *   - SLEEPING    → 0                    (all off — null-guard in core forces hard-off)
 *   - RESET       → 0                    (all off)
 */
inline constexpr LightBitmask kRunLevelMask[kRunLevelCount] = {
  /* IDLE        */ LightBit::PARKING_ON,
  /* STARTING    */ LightBit::PARKING_ON | LightBit::CAB_ON,
  /* RUNNING     */ LightBit::CAB_ON,
  /* TURNING_OFF */ LightBit::PARKING_ON | LightBit::CAB_ON,
  /* SLEEPING    */ 0u,
  /* RESET       */ 0u,
};


// =============================================================================
// 4. CHANNEL DESCRIPTORS
// =============================================================================
//   {name,          channel,   activeMask,                                  brt, pkBrt, dim, type,               dipDim, beacon,       xenon,   indicator}
inline constexpr LightCfg kLedDescriptors[LED_CH_COUNT] = {
  { "headLight",   LED_HEAD,    LightBit::LOW_BEAM_ON | LightBit::FLASHER,  255u,   3u,  0u, LedChannelType::PLAIN_PWM, true,  nullptr,       nullptr, nullptr    },
  { "tailLight",   LED_TAIL,    LightBit::LOW_BEAM_ON,                       30u,   3u, 15u, LedChannelType::TAIL,      false, nullptr,       nullptr, nullptr    },
  { "indicatorL",  LED_IND_L,   LightBit::IND_L,                            255u,   0u,  0u, LedChannelType::INDICATOR, false, nullptr,       nullptr, &kIndLCfg  },
  { "indicatorR",  LED_IND_R,   LightBit::IND_R,                            255u,   0u,  0u, LedChannelType::INDICATOR, false, nullptr,       nullptr, &kIndRCfg  },
  { "fogLight",    LED_FOG,     LightBit::FOG_ON,                           200u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr    },
  { "reverseLight",LED_REVERSE, LightBit::REVERSING,                        140u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr    },
  { "roofLight",   LED_ROOF,    LightBit::PARKING_ON,                       130u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr    },
  { "sideLight",   LED_SIDE,    LightBit::PARKING_ON,                       150u,   0u, 75u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr    },
  { "beacon1",     LED_BEACON1, LightBit::ROOF,                             255u,   0u,  0u, LedChannelType::BEACON,    false, &kBeaconBlue1, nullptr, nullptr    },
  { "beacon2",     LED_BEACON2, LightBit::ROOF,                             255u,   0u,  0u, LedChannelType::BEACON,    false, &kBeaconBlue2, nullptr, nullptr    },
  { "brakeLight",  LED_BRAKE,   0x0000u,                                    255u,   0u,  0u, LedChannelType::TAIL,      false, nullptr,       nullptr, nullptr    },
  { "cabLight",    LED_CAB,     LightBit::CAB_ON,                           100u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr    },
};


// =============================================================================
// 5. MODULE CONFIG
// =============================================================================

/// @brief Module-level static config for the Dumper Truck light module.
inline constexpr LightModuleCfg kLightModuleCfg = {
  .runLevelMask             = kRunLevelMask, ///< RunLevel → LightBit permission table.
  .flickeringWhileCranking  = false,         ///< Flicker all channels while engine cranking.
  .hazardsWhile5thWheelOpen = true,          ///< Auto-hazards when 5th-wheel unlocked.
  .neopixel                 = &kNeopixelCfg, ///< Neopixel bar config (nullptr = bar absent).
};

// EOF dumper_truck_lights.h
