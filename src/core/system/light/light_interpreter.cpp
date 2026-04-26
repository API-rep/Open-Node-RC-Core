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
// 2. IMPLEMENTATION
// =============================================================================


/**
 * @brief Translates a ComBus snapshot into LightState (runtimeMask, runLevelMask).
 *
 * @details
 *   The function follows three clear steps:
 *     1. Capture ComBus inputs into local bools (readability, no repeated indexing).
 *     2. Compute the permission mask (runLevelMask) according to the current RunLevel.
 *     3. Build the runtimeMask (active bits) from flags, RunLevel, and ComBus channels.
 *
 * @note
 *   All local bool variables are temporary aliases for clarity and safety:
 *   they capture the value of a ComBus channel or condition, are never stored in global RAM,
 *   and are inlined by the compiler (no memory cost).
 *
 * @param bus   Current ComBus snapshot (read-only)
 * @param mod   Module-level config (runLevelMask table, skip options...)
 * @param state LightState to fill (runtimeMask, runLevelMask)
 */
void light_interp_update(const ComBus& bus, const LightModuleCfg& mod, LightState& state) {

    // --- 1. Capture ComBus inputs into local bools (readability, no repeated indexing)
  const bool highBeam       = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::HIGH_BEAM)].value;
  const bool flasher        = false;  // TBD: no dedicated FLASHER channel yet
  const bool indicatorLeft  = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::INDICATOR_LEFT)].value;
  const bool indicatorRight = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::INDICATOR_RIGHT)].value;
  const bool hazard         = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::HAZARDS)].value;
  const bool roofLight      = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::ROOF_LIGHT)].value;
  const bool engineRunning  = (bus.runLevel == RunLevel::RUNNING || bus.runLevel == RunLevel::TURNING_OFF);
  const uint16_t centre     = static_cast<uint16_t>(bus.analogBusMaxVal / 2u);
  const bool escInReverse   = (bus.analogBus[static_cast<uint8_t>(AnalogComBusID::ESC_SPEED_BUS)].value < centre);
  const bool escIsBraking   = bus.digitalBus[static_cast<uint8_t>(DigitalComBusID::BRAKING)].value;
  const bool batteryLow     = bus.batteryIsLow;

  // --- 2. Compute the permission mask (runLevelMask) according to current RunLevel
  const uint8_t rl = (bus.runLevel >= RunLevel::IDLE) ? static_cast<uint8_t>(bus.runLevel) : 0u;
  state.runLevelMask = mod.runLevelMask[rl];

  // --- 3. Build the runtimeMask (active bits) from flags, RunLevel, and ComBus
  const bool forcedHazard = hazard || batteryLow;
  LightBitmask mask = 0u;

  // RunLevel auto-bits (structural lights driven by machine state)
  if (bus.runLevel == RunLevel::IDLE){
    mask |= LightBit::PARKING_ON;
  }

  else if (bus.runLevel == RunLevel::STARTING  ||
           bus.runLevel == RunLevel::RUNNING    ||
           bus.runLevel == RunLevel::TURNING_OFF){
    mask |= LightBit::PARKING_ON | LightBit::CAB_ON;
  }
  
  if (bus.runLevel == RunLevel::STARTING){
    mask |= LightBit::CRANKING;
  }

  // Motion-derived
  if (escInReverse && engineRunning)  mask |= LightBit::REVERSING;
  if (escIsBraking)                   mask |= LightBit::BRAKING;

  // ComBus-commanded  (LOW_BEAM before HIGH_BEAM — dependency order)
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
