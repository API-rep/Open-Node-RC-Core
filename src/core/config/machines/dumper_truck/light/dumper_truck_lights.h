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


// =============================================================================
// 3. RUN-LEVEL MASK TABLE  (layer 2 — permission gate)
// =============================================================================

/**
 * @brief Permission gate: maps each RunLevel (IDLE=0 … RESET=5) to the set of
 *   LightBit:: bits that MAY be active at that RunLevel.
 *
 * @details This is a GATE, not an "auto-on" list.  A channel activates only when
 *   its activeMask has at least one bit present in BOTH this table and runtimeMask.
 *   In core: `active = (activeMask & runLevelMask & runtimeMask) != 0`.
 *
 *   RunLevel permission for the Dumper Truck:
 *   - IDLE        → PARKING_ON + signals     (engine off — no beams, no cab)
 *   - STARTING    → PARKING_ON + CAB + signals  (cranking — no driving lights yet)
 *   - RUNNING     → all bits                 (full lighting available via ComBus)
 *   - TURNING_OFF → PARKING + CAB + LOW_BEAM + signals  (no fog / high-beam)
 *   - SLEEPING    → 0                        (system suspended)
 *   - RESET       → 0                        (boot/reinit)
 */
inline constexpr LightBitmask kRunLevelMask[6] = {
  /* IDLE        */ LightBit::PARKING_ON,
  /* STARTING    */ LightBit::CAB_ON,
  /* RUNNING     */ LightBit::CAB_ON,
  /* TURNING_OFF */ LightBit::CAB_ON,
  /* SLEEPING    */ 0u,
  /* RESET       */ 0u,
};


// =============================================================================
// 4. CHANNEL DESCRIPTORS
// =============================================================================
//   {channel,   activeMask,                                  brt, pkBrt, dim, type,               dipDim, beacon,       xenon,   indicator}
inline constexpr LightCfg kLedDescriptors[LED_CH_COUNT] = {
  { LED_HEAD,    LightBit::LOW_BEAM_ON | LightBit::FLASHER,  255u,   3u,  0u, LedChannelType::PLAIN_PWM, true,  nullptr,       nullptr, nullptr   },
  { LED_TAIL,    LightBit::LOW_BEAM_ON,                       30u,   3u, 15u, LedChannelType::TAIL,      false, nullptr,       nullptr, nullptr   },
  { LED_IND_L,   LightBit::IND_L,                            255u,   0u,  0u, LedChannelType::INDICATOR, false, nullptr,       nullptr, &kIndLCfg },
  { LED_IND_R,   LightBit::IND_R,                            255u,   0u,  0u, LedChannelType::INDICATOR, false, nullptr,       nullptr, &kIndRCfg },
  { LED_FOG,     LightBit::FOG_ON,                           200u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr   },
  { LED_REVERSE, LightBit::REVERSING,                        140u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr   },
  { LED_ROOF,    LightBit::PARKING_ON,                       130u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr   },
  { LED_SIDE,    LightBit::PARKING_ON,                       150u,   0u, 75u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr   },
  { LED_BEACON1, LightBit::ROOF,                             255u,   0u,  0u, LedChannelType::BEACON,    false, &kBeaconBlue1, nullptr, nullptr   },
  { LED_BEACON2, LightBit::ROOF,                             255u,   0u,  0u, LedChannelType::BEACON,    false, &kBeaconBlue2, nullptr, nullptr   },
  { LED_BRAKE,   0x0000u,                                    255u,   0u,  0u, LedChannelType::TAIL,      false, nullptr,       nullptr, nullptr   },
  { LED_CAB,     LightBit::CAB_ON,                           100u,   0u,  0u, LedChannelType::PLAIN_PWM, false, nullptr,       nullptr, nullptr   },
};


// =============================================================================
// 5. MODULE CONFIG
// =============================================================================

/// @brief Module-level static config for the Dumper Truck light module.
inline constexpr LightModuleCfg kLightModuleCfg = {
  .runLevelMask              = kRunLevelMask,    ///< Layer 3: machine RunLevel → LightBit mask table.
  .flickeringWhileCranking   = false, ///< Flicker all channels while engine cranking.
  .skipCabStep               = false, ///< Pass through cab-only step in FSM.
  .skipFogStep               = false, ///< Pass through fog step in FSM.
  .hazardsWhile5thWheelOpen  = true,  ///< Auto-hazards when 5th-wheel unlocked.
  .neopixelCount             =  8u,   ///< Number of WS2812 LEDs on the bar.
  .neopixelBrightness        = 127u,  ///< Global FastLED brightness cap.
  .neopixelMaxPowerMilliAmps = 100u,  ///< FastLED power budget (mA).
  .neopixelMode              =  2u,   ///< 1=Demo 2=KnightRider 3=Bluelight 4=UnionJack 5=B33lz3bub
  .neopixelAsHighBeam        = true,  ///< Fill bar white when high-beam active.
};

// EOF dumper_truck_lights.h
