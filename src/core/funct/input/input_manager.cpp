/**
 * @file input_manager.cpp
 * @brief Implementation of input processing and bus synchronization
 */

#include "input_manager.h"
#include <core/config/combus/combus.h>
#include <PS4Controller.h>


/**
 * @brief Initialize input hardware/protocol
 */

void input_setup() {
#if INPUT_MODULE == PS4_DS4_BT
  PS4.begin(PS4_BLUETOOTH_ADDRESS);
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
      // Reset analog channels drive status
    for (uint8_t i = 0; i < InputAnalogMapCount; i++) {
      uint8_t ch = static_cast<uint8_t>(InputAnalogMapArray[i].busChannel);
      bus.analogBus[ch].isDrived = false;
    }

      // Reset digital channels drive status
    for (uint8_t i = 0; i < InputDigitalMapCount; i++) {
      uint8_t ch = static_cast<uint8_t>(InputDigitalMapArray[i].busChannel);
      bus.digitalBus[ch].isDrived = false;
    }
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
    bus.analogBus[ch].value = m.isInverted ? (bus.analogBusMaxVal - val) : val;
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
    bus.digitalBus[ch].value = finalState;
    bus.digitalBus[ch].isDrived = true;
  }

// ==========================================================
// 4. PERIODIC DEBUG
// ==========================================================

  static unsigned long lastDebugTM = 0;
  if (millis() - lastDebugTM > 1000) {
    LOG_INPUT_DEBUG(bus);
    lastDebugTM = millis();
  }

#endif
}
