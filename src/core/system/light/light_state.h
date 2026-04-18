/*!****************************************************************************
 * @file  light_state.h
 * @brief Light module — channel descriptor types, runtime state, and module config.
 *
 * @details Five distinct type groups live here:
 *
 *   BeaconCfg / XenonCfg / IndicatorCfg — optional per-channel behavioural
 *       sub-structs (pointer nullptr = feature inactive).
 *
 *   LedChannelType — dispatch enum selecting which parser handler applies.
 *
 *   LightCfg  — compile-time per-channel config (constexpr) with optional
 *       sub-struct pointers; runtime state managed by the statusLED* backend.
 *
 *   LightModuleCfg — compile-time module-level config (global flags, neopixel
 *       params). One constexpr instance per machine (e.g. kLightModuleCfg).
 *
 *   LightState     — runtime working state updated each loop() pass.
 *       Written by light_interpreter, read by light_core.
 *
 *   LightLocalBus  — preparatory local sub-bus (not transmitted on wire).
 *
 * Consumers:
 *   - dumper_truck_lights.h  — defines kLedDescriptors[] + kLightModuleCfg (using LightBit:: masks)
 *   - light_interpreter.h    — writes LightState from ComBus
 *   - light_core.h           — iterates LightCfg[], drives statusLED / FastLED
 *   - light_update.h         — public API (future)
 *******************************************************************************///
#pragma once

#include <cstdint>


// =============================================================================
// 1. BEHAVIOURAL SUB-STRUCTS  (optional per-channel modifiers)
// =============================================================================

/**
 * @brief Beacon / gyrophare flash pattern config.
 *
 * @note Pass as the `beacon` pointer in LightCfg for BEACON-type channels.
 *   `phaseOffsetMs` staggers two simultaneous beacons (e.g. +330 ms for the
 *   second unit of a paired blue-light set).
 */
struct BeaconCfg {
  uint16_t onMs;          ///< Flash on duration (ms).
  uint16_t offMs;         ///< Flash off duration (ms).
  uint16_t pauseMs;       ///< Pause between pulse bursts (ms); 0 = continuous.
  uint8_t  pulses;        ///< Pulses per burst; 0 = single continuous flash.
  int16_t  phaseOffsetMs; ///< Timing offset vs. sibling beacon (ms); 0 = in-phase.
};


/**
 * @brief Xenon headlight ignition-flash overlay config.
 *
 * @note Pass as the `xenon` pointer in LightCfg for PLAIN_PWM channels that
 *   need an ignition flash.  The flash is triggered on the channel's rising edge
 *   (off→on transition) and boosts brightness by `flashBoost` for `flashMs` ms.
 */
struct XenonCfg {
  uint16_t flashMs;    ///< Ignition-flash hold duration (ms).
  uint8_t  flashBoost; ///< Additive brightness boost during flash (0–255).
};


/**
 * @brief Indicator / turn-signal per-channel config.
 *
 * @note Pass as the `indicator` pointer in LightCfg for INDICATOR-type channels.
 *   Set `isRight = true` to read state.indicatorRight (use to correct swapped wiring
 *   without a module-level swap flag).  `sideMarker = true` enables US side-marker
 *   mode: channel dims to `parkBrightness` between flashes when lights are on.
 */
struct IndicatorCfg {
  uint16_t onMs;       ///< Flash on duration (ms).
  uint16_t offMs;      ///< Flash off duration (ms).
  bool     ledType;    ///< True = hard cutover (no ramp); false = incandescent ramp.
  bool     sideMarker; ///< US mode: stay on dimmed between flashes when lights are on.
  ///< Channel side is encoded in activeMask (LightBit::IND_L or IND_R) — no isRight field needed.
};


// =============================================================================
// 2. RUNTIME MASK BIT DEFINITIONS
// =============================================================================

/// @brief Bitmask type for light activation masks.  All LightBit:: constants,
///   activeMask, runLevelMask, and runtimeMask fields use this type.
///   Defined as uint16_t — supports up to 16 independent light functions.
///   Widen to uint32_t here when more than 16 bits are needed.
using LightBitmask = uint16_t;

/**
 * @brief Named bit positions for the LightBitmask runtime light mask.
 *
 * @details The interpreter builds `LightState::runtimeMask` each loop pass.
 *   The parser AND-s each channel's `activeMask` against it to decide brightness —
 *   no separate per-type switch logic needed.
 *   `channel ON  =  (activeMask & runtimeMask) != 0`
 *
 */
namespace LightBit {

    // --- Lights (bits 0-7) --------------------------------------------------
  static constexpr LightBitmask CAB_ON        = 1u << 0;  ///< Cab / interior light active (step 1, 2, 5).
  static constexpr LightBitmask LOW_BEAM_ON   = 1u << 1;  ///< Low beam active (step >= 3).
  static constexpr LightBitmask FOG_ON        = 1u << 2;  ///< Fog lights active (step >= 4).
  static constexpr LightBitmask HIGH_BEAM     = 1u << 3;  ///< High beam — only set when LOW_BEAM_ON is also active.
  static constexpr LightBitmask FLASHER       = 1u << 4;  ///< Momentary headlight flasher (ComBus command).
  static constexpr LightBitmask ROOF          = 1u << 5;  ///< Roof light / beacon (ComBus command).
  static constexpr LightBitmask IND_L         = 1u << 6;  ///< Left turn indicator (also set by hazard / batteryLow).
  static constexpr LightBitmask IND_R         = 1u << 7;  ///< Right turn indicator (also set by hazard / batteryLow).

    // --- Machine states (bits 8-11) ---------------------------------------
  static constexpr LightBitmask PARKING_ON    = 1u << 8;  ///< Parking / side markers active.
  static constexpr LightBitmask REVERSING     = 1u << 9;  ///< ESC reversing AND engine running / cranking.
  static constexpr LightBitmask BRAKING       = 1u << 10; ///< ESC braking.
  static constexpr LightBitmask CRANKING      = 1u << 11; ///< Engine cranking (RunLevel::STARTING) — drives xenon / flicker in light_core.

} // namespace LightBit


// =============================================================================
// 3. LED CHANNEL DESCRIPTOR
// =============================================================================

/**
 * @brief LED channel type — selects the parser dispatch handler.
 *
 * @details REVERSING and HIGHBEAM have been merged into PLAIN_PWM: encode their
 *   activation condition via `activeMask` bits (LightBit::REVERSING,
 *   LightBit::HIGH_BEAM | LightBit::FLASHER) instead.
 */
enum class LedChannelType : uint8_t {
  PLAIN_PWM  = 0u, ///< Step-based brightness driven by activeMask AND runtimeMask.
  INDICATOR,       ///< Flash driven by activeMask AND runtimeMask (IND_L/IND_R bits). Needs IndicatorCfg.
  BEACON,          ///< Flash driven by activeMask AND runtimeMask (ROOF bit). Needs BeaconCfg.
  TAIL,            ///< Like PLAIN_PWM but drives brightness=255 when BRAKING bit set.
};


/**
 * @brief Compile-time descriptor for one LED channel.
 *
 * @details Two logical sections:
 *   - **Compile-time config** (all fields, ~24 B flash per channel): brightness levels,
 *     step masks, dispatch type, and optional sub-struct pointers.
 *   - **Runtime state**: managed by the `statusLED*` backend via `soundLeds[]`.
 *     No additional RAM section needed here until the winter 2026 rework.
 *
 *   ### activeMask (16-bit LightBit bitmask)
 *   `activeMask & runtimeMask` non-zero → drive at `brightness`.
 *   `parkBrightness > 0` implicitly enables a park mode: when `PARKING_ON` is set
 *   but `activeMask` is not matched, the channel dims to `parkBrightness`.
 *   No separate `parkMask` field — parking is always tied to `LightBit::PARKING_ON`.
 *   INDICATOR and BEACON types interpret the mask as an enable gate: non-zero = flash.
 *
 *   ### Sub-struct pointers
 *   A `nullptr` pointer means that feature is inactive; the pointer doubles as a
 *   presence flag so no separate `bool hasBeacon` / `bool hasXenon` fields are needed.
 */
struct LightCfg {

  // --- Compile-time config (flash) ----------------------------------------
  uint8_t         lightChannel;    ///< soundLeds[] index — matches the board Light enum value.
  LightBitmask    activeMask;      ///< Bitmask AND runtimeMask → drive at \c brightness. Use LightBit:: constants.
  uint8_t         brightness;      ///< Full-on target brightness (0–255).
  uint8_t         parkBrightness;  ///< Parking-mode brightness (0–255). 0 = no park mode. Active when PARKING_ON set but activeMask not matched.
  uint8_t         dimFloor;        ///< Floor brightness after cranking dim (0 = no floor).
  LedChannelType  type;            ///< Parser dispatch key — see LedChannelType.
  bool            applyDipDim;     ///< Reduce brightness by dipDim when not in high-beam mode (head light).

  // --- Optional sub-struct pointers (nullptr = feature inactive) ----------
  const BeaconCfg*    beacon;    ///< Beacon pattern; required when type == BEACON.
  const XenonCfg*     xenon;     ///< Xenon overlay; valid on PLAIN_PWM type.
  const IndicatorCfg* indicator; ///< Indicator config; required when type == INDICATOR.
};


// =============================================================================
// 4. RUNTIME STATE
// =============================================================================

/**
 * @brief Light runtime state. Updated by light_interpreter each loop() pass.
 *
 * @details Minimal set passed from light_interpreter to light_core.
 *   All mask bits are pre-computed by the interpreter from ComBus;
 *   light_core reads only `runLevelMask`, `runtimeMask`, and `lightsState`
 *   — it never touches ComBus directly.
 *   Entirely on Core 1 — no volatile qualifier needed.
 */
struct LightState {

  // ---- Computed masks (built by light_interpreter each loop pass) ----------
  LightBitmask runLevelMask;     ///< Layer 2 — permission gate: which LightBit:: bits are ALLOWED at the current RunLevel.
  LightBitmask runtimeMask;      ///< Layer 3 — commanded bits: RunLevel auto-bits + ComBus channel parsing.
  ///< Channel active = (activeMask & runLevelMask & runtimeMask) != 0  — computed in light_core.

  // ---- Main light FSM ----------------------------------------------------
  uint8_t lightsState;           ///< 6-step manual FSM (0=off … 5=full).

};


// =============================================================================
// 3. MODULE-LEVEL CONFIGURATION
// =============================================================================

/**
 * @brief Module-level light config — parameters that apply globally across all channels.
 *
 * @details Fields that cannot be expressed per LightCfg channel are grouped here:
 *   - `skipCabStep` / `skipFogStep` prevent "empty" button presses when the corresponding
 *     light circuit is absent; the interpreter skips those FSM steps.
 *   - Cranking flicker is global (all channels share the same crankingDim offset).
 *   - Neopixel params apply to the whole strip, not per channel.
 *
 *   Instantiated once as `inline constexpr` in the machine light config header.
 */
struct LightModuleCfg {
  const LightBitmask* runLevelMask;    ///< Permission gate table[6], one entry per RunLevel. Entry i = set of LightBit:: bits PERMITTED at that RunLevel. AND-ed with runtimeMask in core.
  bool     flickeringWhileCranking;    ///< All channels flicker (random dim) while engine cranking.
  bool     skipCabStep;                ///< Skip FSM step 1 (cab only) when cab light is not wired.
  bool     skipFogStep;                ///< Skip FSM step 4 (fog) when fog light is not wired.
  bool     hazardsWhile5thWheelOpen;   ///< Trigger hazards when 5th-wheel is unlocked (TBD).
  uint8_t  neopixelCount;             ///< Number of WS2812 LEDs on the bar (0 = neopixel disabled).
  uint8_t  neopixelBrightness;        ///< Global FastLED brightness cap (0–255).
  uint16_t neopixelMaxPowerMilliAmps; ///< FastLED power budget (mA).
  uint8_t  neopixelMode;             ///< Strip animation: 1=Demo 2=KnightRider 3=Bluelight 4=UnionJack 5=B33lz3bub.
  bool     neopixelAsHighBeam;        ///< Fill bar white when high-beam or flasher is active.
};


// =============================================================================
// 4. LOCAL SUB-BUS  (preparatory — ComBus v2 architecture)
// =============================================================================

/**
 * @brief Light module local sub-bus — not transmitted on the wire.
 *
 * @details Preparatory struct for the ComBus v2 layered architecture (see
 *   copilot-instructions.md §5 "ComBus v2 — layered bus architecture").
 *   Currently wraps LightState (interpreter output).  Runtime state for
 *   individual LightCfg channels (timers, phase counters) will move here
 *   during the winter 2026 rework alongside the statusLED → class migration.
 */
struct LightLocalBus {
  LightState state;  ///< Updated by light_interpreter each loop pass; read by light_core.
};

// EOF light_state.h
