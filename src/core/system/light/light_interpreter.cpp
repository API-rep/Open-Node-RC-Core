/******************************************************************************
 * @file light_interpreter.cpp
 * @brief Light module — ComBus → LightState implementation.
 *
 * @details Channel indices are resolved via combus_types.h which dispatches
 *   to the machine-specific enum file and re-exports it with
 *   `using namespace <Machine>` — no manual index constants.
 *****************************************************************************/

#ifdef LIGHT_ENABLE

#include "light_interpreter.h"
#include <core/config/machines/combus_types.h>  ///< ComBus externs + AnalogComBusID / DigitalComBusID (using namespace)


// =============================================================================
// 1. FILE-SCOPE STATE  (edge-detect only — no output state here)
// =============================================================================

static bool    s_prevLights   = false;  ///< Previous LIGHTS channel value — rising-edge detect.
static uint8_t s_lightsState  = 0u;    ///< Current 6-step light FSM position.


// =============================================================================
// 2. IMPLEMENTATION
// =============================================================================

void light_interp_update(const ComBus& bus, const LightModuleCfg& mod, LightState& state) {

    // --- Light FSM: 6-step cycle on LIGHTS rising edge ---
  const bool lights = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::LIGHTS)].value;
  if (lights && !s_prevLights) {
    s_lightsState = static_cast<uint8_t>((s_lightsState + 1u) % 6u);
  }
  s_prevLights      = lights;
  state.lightsState = s_lightsState;

    // --- Beam flags ---
  const bool highBeam = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::HIGH_BEAM)].value;
  const bool flasher  = false;  // TBD: no dedicated FLASHER channel yet

    // --- Indicators / hazards ---
  const bool indicatorLeft  = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::INDICATOR_LEFT)].value;
  const bool indicatorRight = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::INDICATOR_RIGHT)].value;
  const bool hazard         = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::HAZARDS)].value;

    // --- Roof / special lights ---
  const bool roofLight = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::ROOF_LIGHT)].value;

    // --- Engine state (derived from RunLevel) ---
  const bool engineRunning = (bus.runLevel == RunLevel::RUNNING ||
                              bus.runLevel == RunLevel::TURNING_OFF);

    // --- Direction (ESC_SPEED_BUS vs centre; below centre = reverse) ---
  const uint16_t centre      = static_cast<uint16_t>(bus.analogBusMaxVal / 2u);
  const bool     escInReverse = (bus.analogBus[static_cast<uint8_t>(AnalogComBusID::ESC_SPEED_BUS)].value < centre);

    // --- Braking (MotionOutput::isBraking written to BRAKING digital channel) ---
  const bool escIsBraking = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::BRAKING)].value;

    // --- System flags ---
  const bool batteryLow = bus.batteryIsLow;

    // --- FSM step-skip (prevents empty button presses when circuit not wired) ---
  if (s_lightsState == 1u && mod.skipCabStep)  s_lightsState = 2u;
  if (s_lightsState == 4u && mod.skipFogStep)  s_lightsState = 5u;
  if (s_lightsState == 5u && mod.skipCabStep)  s_lightsState = 0u;
  state.lightsState = s_lightsState;

    // --- Layer 2 : permission gate (stored in state; AND-ed with runtimeMask in core) ---
  const uint8_t rl = (bus.runLevel >= RunLevel::IDLE)
                     ? static_cast<uint8_t>(bus.runLevel) : 0u;
  state.runLevelMask = mod.runLevelMask[rl];

    // --- Layer 3 : commanded bits → runtimeMask ---
  const bool forcedHazard = hazard || batteryLow;
  LightBitmask mask       = 0u;

    //   RunLevel auto-bits (structural lights driven by machine state)
  if (bus.runLevel == RunLevel::IDLE)
      mask |= LightBit::PARKING_ON;
  else if (bus.runLevel == RunLevel::STARTING  ||
           bus.runLevel == RunLevel::RUNNING    ||
           bus.runLevel == RunLevel::TURNING_OFF)
      mask |= LightBit::PARKING_ON | LightBit::CAB_ON;
  if (bus.runLevel == RunLevel::STARTING)
      mask |= LightBit::CRANKING;

    //   Motion-derived
  if (escInReverse && engineRunning)  mask |= LightBit::REVERSING;
  if (escIsBraking)                   mask |= LightBit::BRAKING;

    //   ComBus-commanded  (LOW_BEAM before HIGH_BEAM — dependency order)
  if (bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::LOW_BEAM)].value)
                                                mask |= LightBit::LOW_BEAM_ON;
  if (highBeam && (mask & LightBit::LOW_BEAM_ON))  mask |= LightBit::HIGH_BEAM;
  if (indicatorLeft  || forcedHazard)              mask |= LightBit::IND_L;
  if (indicatorRight || forcedHazard)              mask |= LightBit::IND_R;
  if (flasher)                                      mask |= LightBit::FLASHER;
  if (roofLight)                                    mask |= LightBit::ROOF;

  state.runtimeMask = mask;
}

#endif  // LIGHT_ENABLE

// EOF light_interpreter.cpp

// EOF light_interpreter.cpp
