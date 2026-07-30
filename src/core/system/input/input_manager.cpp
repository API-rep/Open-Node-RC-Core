/**
 * @file input_manager.cpp
 * @brief Implementation of core-side input acquisition
 */

#include "input_manager.h"

int16_t inputAnalogValue[static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)]   = {0};
bool    inputDigitalValue[static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)] = {false};

static bool inputConnected = false;


/**
 * @brief Initialize input hardware/protocol
 */

void input_setup() {
#if defined(INPUT_PS4_DS4_BT)

  sys_log_info("[INPUT] BT stack init...\n");
  PS4.begin(PS4_BLUETOOTH_ADDRESS);
  sys_log_info("[INPUT] BT stack init complete — waiting for controller (%s)\n", PS4_BLUETOOTH_ADDRESS);

#elif defined(INPUT_NONE)
  sys_log_warn("[INPUT] No input module configured — machine running in autonomous/headless mode.\n");
#endif
}


/**
 * @brief Acquire + process physical input device state (core-only)
 */

void input_refresh() {
#if defined(INPUT_PS4_DS4_BT)

  inputConnected = PS4.isConnected();
  if (!inputConnected) {
    return;   // stale values retained — machine-side input_update() applies failsafe
  }

// ==========================================================
// ANALOG ACQUISITION + DEADBAND
// ==========================================================

  for (uint8_t i = 0; i < static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT); i++) {
    const AnalogInputDevID id = static_cast<AnalogInputDevID>(i);
    int16_t raw = 0;

    switch (id) {
      case AnalogInputDevID::LX_STICK:  raw = PS4.LStickX(); break;
      case AnalogInputDevID::LY_STICK:  raw = PS4.LStickY(); break;
      case AnalogInputDevID::RX_STICK:  raw = PS4.RStickX(); break;
      case AnalogInputDevID::RY_STICK:  raw = PS4.RStickY(); break;
      case AnalogInputDevID::L2_BUTTON: raw = PS4.L2Value(); break;
      case AnalogInputDevID::R2_BUTTON: raw = PS4.R2Value(); break;
      default: break;
    }

    const AnalogInputDev &dev = inputDev.analogInputDev[i];

    if (dev.deadband > 0) {
      if (dev.type == RemoteComp::ANALOG_BUTTON && raw <= (int16_t)dev.deadband) {
        raw = (int16_t)dev.minVal;   // clamp to rest position
      } else if (dev.type == RemoteComp::ANALOG_STICK) {
        const int32_t center = (dev.minVal + dev.maxVal) / 2;
        if (raw >= (int16_t)(center - dev.deadband) && raw <= (int16_t)(center + dev.deadband))
          raw = (int16_t)center;     // clamp to mechanical center
      }
    }

    inputAnalogValue[i] = raw;
  }

// ==========================================================
// DIGITAL ACQUISITION (raw, unprocessed)
// ==========================================================

  for (uint8_t i = 0; i < static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT); i++) {
    const DigitalInputDevID id = static_cast<DigitalInputDevID>(i);
    bool raw = false;

    switch (id) {
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

    inputDigitalValue[i] = raw;
  }

#endif
}

bool input_is_connected() {
#if defined(INPUT_PS4_DS4_BT)
  return inputConnected;
#else
  return false;
#endif
}

const char* input_get_name() {
#if defined(INPUT_PS4_DS4_BT)
  return inputDev.infoName;
#else
  return "---";
#endif
}

// EOF input_manager.cpp