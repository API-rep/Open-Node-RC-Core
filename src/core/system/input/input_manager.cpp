/**
 * @file input_manager.cpp
 * @brief Implementation of input processing and bus synchronization
 */

#include "input_manager.h"
#include <core/config/combus/combus_types.h>
#include <core/system/combus/combus_manager.h>
#include <core/system/combus/combus_access.h>
#include <PS4Controller.h>


/**
 * @brief Initialize input hardware/protocol
 */

void input_setup() {
#if INPUT_MODULE == PS4_DS4_BT
  sys_log_info("[INPUT] BT stack init...\n");
  PS4.begin(PS4_BLUETOOTH_ADDRESS);
  sys_log_info("[INPUT] BT stack init complete — waiting for controller (%s)\n", PS4_BLUETOOTH_ADDRESS);
#endif
}



/**
 * @brief Main input processing loop
 */

void input_update(ComBus &bus) {
#if INPUT_MODULE == PS4_DS4_BT

// ==========================================================
// 1. SIGNAL LOSS & FAILSAFE MANAGEMENT
// ==========================================================

  if (!PS4.isConnected()) {
    resetComBusDriveFlags(bus);
    return;
  }

// ==========================================================
// 2. ANALOG INPUTS MAPPING ENGINE
// ==========================================================

  for (uint8_t i = 0; i < InputAnalogMapCount; i++) {

      // --- Safe data access ---
    const InputAnalogMap m = InputAnalogMapArray[i];
    uint8_t devID = static_cast<uint8_t>(m.devID);
    int16_t raw = 0;

      // --- Physical Acquisition ---
    switch (m.devID) {
      case AnalogInputDevID::LX_STICK:  raw = PS4.LStickX(); break;
      case AnalogInputDevID::LY_STICK:  raw = PS4.LStickY(); break;
      case AnalogInputDevID::RX_STICK:  raw = PS4.RStickX(); break;
      case AnalogInputDevID::RY_STICK:  raw = PS4.RStickY(); break;
      case AnalogInputDevID::L2_BUTTON: raw = PS4.L2Value(); break;
      case AnalogInputDevID::R2_BUTTON: raw = PS4.R2Value(); break;
      default: break;
    }

      // --- Dynamic Scaling ---
    const AnalogInputDev &dev = inputDev.analogInputDev[devID];
    uint16_t val = map(raw, dev.minVal, dev.maxVal, 0, bus.analogBusMaxVal);

      // --- ComBus Injection ---
    uint8_t ch = static_cast<uint8_t>(m.busChannel);
    combus_set_analog(bus, static_cast<AnalogComBusID>(ch), m.isInverted ? (bus.analogBusMaxVal - val) : val, ChanOwner::INPUT_DEV);
    bus.analogBus[ch].isDrived = true;
  }

// ==========================================================
// 3. DIGITAL INPUTS MAPPING ENGINE
// ==========================================================

  for (uint8_t i = 0; i < InputDigitalMapCount; i++) {

      // --- Safe data access ---
    const InputDigitalMap m = InputDigitalMapArray[i];
    uint8_t devID = static_cast<uint8_t>(m.devID);
    bool raw = false;

      // --- Physical Acquisition ---
    switch (m.devID) {
      case DigitalInputDevID::SQUARE_BTN:    raw = PS4.Square(); break;
      case DigitalInputDevID::CROSS_BTN:     raw = PS4.Cross(); break;
      case DigitalInputDevID::CIRCLE_BTN:    raw = PS4.Circle(); break;
      case DigitalInputDevID::TRIANGLE_BTN:  raw = PS4.Triangle(); break;
      case DigitalInputDevID::L1_BTN:        raw = PS4.L1(); break;
      case DigitalInputDevID::R1_BTN:        raw = PS4.R1(); break;
      case DigitalInputDevID::L2_BTN:        raw = PS4.L2(); break;
      case DigitalInputDevID::R2_BTN:        raw = PS4.R2(); break;
      case DigitalInputDevID::UP_ARROW:      raw = PS4.Up(); break;
      case DigitalInputDevID::RIGHT_ARROW:   raw = PS4.Right(); break;
      case DigitalInputDevID::DOWN_ARROW:    raw = PS4.Down(); break;
      case DigitalInputDevID::LEFT_ARROW:    raw = PS4.Left(); break;
      case DigitalInputDevID::SHARE_BTN:     raw = PS4.Share(); break;
      case DigitalInputDevID::OPTIONS_BTN:   raw = PS4.Options(); break;
      case DigitalInputDevID::PS_BTN:        raw = PS4.PSButton(); break;
      case DigitalInputDevID::TOUCHPAD_BTN:  raw = PS4.Touchpad(); break;
      case DigitalInputDevID::L_STICK_BTN:   raw = PS4.L3(); break;
      case DigitalInputDevID::R_STICK_BTN:   raw = PS4.R3(); break;
      default: break;
    }

      // --- Logic Processing ---
    const DigitalInputDev &dev = inputDev.digitalInputDev[devID];
    bool finalState = (raw != (m.isInverted || dev.isInverted));

      // --- ComBus Injection ---
    uint8_t ch = static_cast<uint8_t>(m.busChannel);
    combus_set_digital(bus, static_cast<DigitalComBusID>(ch), finalState, ChanOwner::INPUT_DEV);
    bus.digitalBus[ch].isDrived = true;
  }

  // Keep combus watchdog alive
  bus.lastFrameMs = millis();

#endif
}


const char* input_get_name() {
#if INPUT_MODULE == PS4_DS4_BT
  return inputDev.infoName;
#else
  return "---";
#endif
}
