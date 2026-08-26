/**
 * @file input_update.cpp
 * @brief Implementation of machine-side input -> ComBus mapping
 */

#include "input_update.h"

#include <core/system/input/input_manager.h>   // input_is_connected(), inputAnalogValue[], inputDigitalValue[], inputDev
#include <core/system/combus/combus_access.h>  // combus_set_analog, combus_set_digital
#include <core/config/machines/dumper_truck/inputs/inputs_map/inputs_map.h>  // A9.9: default mapping (replaces instance-specific inputs_map.h)

void input_update(ComBus &bus) {

// ==========================================================
// SIGNAL LOSS & FAILSAFE MANAGEMENT
// ==========================================================

  if (!input_is_connected()) {
      // Force every mapped channel to a safe neutral value, bypassing
      // mapping-level inversion — "no signal" is not a direction.
    for (uint8_t i = 0; i < InputAnalogMapCount; i++) {
      const InputAnalogMap &m   = InputAnalogMapArray[i];
      const AnalogInputDev &dev = inputDev.analogInputDev[static_cast<uint8_t>(m.devID)];

      int16_t restRaw = (dev.type == RemoteComp::ANALOG_BUTTON)
                         ? (int16_t)dev.minVal
                         : (int16_t)((dev.minVal + dev.maxVal) / 2);
      uint16_t neutral = (uint16_t)map(restRaw, dev.minVal, dev.maxVal, 0, bus.analogBusMaxVal);
      combus_set_analog(bus, m.busChannel, neutral, ChanLayer::LOCAL);
    }

    for (uint8_t i = 0; i < InputDigitalMapCount; i++) {
      combus_set_digital(bus, InputDigitalMapArray[i].busChannel, false, ChanLayer::LOCAL);
    }
    return;   // source inactive — isDrived left as pre-cleared by sys_manager
  }

// ==========================================================
// ANALOG MAPPING: input device value -> ComBus channel
// ==========================================================

  for (uint8_t i = 0; i < InputAnalogMapCount; i++) {
    const InputAnalogMap &m = InputAnalogMapArray[i];
    uint8_t devID = static_cast<uint8_t>(m.devID);
    const AnalogInputDev &dev = inputDev.analogInputDev[devID];

    uint16_t val    = map(inputAnalogValue[devID], dev.minVal, dev.maxVal, 0, bus.analogBusMaxVal);
    uint16_t busVal = m.isInverted ? (bus.analogBusMaxVal - val) : val;

    combus_set_analog(bus, m.busChannel, busVal, ChanLayer::LOCAL);
  }

// ==========================================================
// DIGITAL MAPPING: input device value -> ComBus channel
// ==========================================================

  for (uint8_t i = 0; i < InputDigitalMapCount; i++) {
    const InputDigitalMap &m = InputDigitalMapArray[i];
    uint8_t devID = static_cast<uint8_t>(m.devID);
    const DigitalInputDev &dev = inputDev.digitalInputDev[devID];

    bool raw        = inputDigitalValue[devID];
    bool finalState = (raw != (m.isInverted || dev.isInverted));

    combus_set_digital(bus, m.busChannel, finalState, ChanLayer::LOCAL);
  }

    // --- Mark bus as driven by this physical source ---
  bus.isDrived    = true;
  bus.lastFrameMs = millis();
}

// EOF input_update.cpp