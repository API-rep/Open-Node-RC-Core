/*!****************************************************************************
 * @file  PS4_dualshock4.h
 * @brief PS4 dualshock4 BT remote control config file.
 * This file store :
 * - Hardware setup:
 *    -> Type (analog sticks, button, etc ... must match predefined structure in truct.h)
 *    -> input value (limits, resolution ...)
 * - Protocol used for communication
 * 
 * Could be shared by DIY remote project and/or vehicle diy project code.
 *******************************************************************************/// 
#pragma once

#include <defs/remotes_defs.h>  // RemoteProtocol
#include <struct/remotes_struct.h>  // InputDev, AnalogInputDev, DigitalInputDev
#include <PS4Controller.h>  // PS4 lib if PS4_DS4_BT set

// =============================================================================
// DEVICE CONSTANTS
// =============================================================================

#define DEF_STICK_MIN_VAL  -127          // maximum negative value for sticks
#define DEF_STICK_MAX_VAL   127          // maximum positive value for sticks

#define DEF_ANALOG_BUTTON_MIN_VAL    0   // maximum negative value for analog button
#define DEF_ANALOG_BUTTON_MAX_VAL  255   // maximum positive value for analog button


// =============================================================================
// ANALOG DEVICE ENUMS AND DESCRIPTOR ARRAYS — EMPTY (COUNT = 0)
// =============================================================================

enum class AnalogInputDevID : uint8_t {
  LX_STICK = 0,
  LY_STICK,
  RX_STICK,
  RY_STICK,
  L2_BUTTON,
  R2_BUTTON,
  ANALOG_DEV_COUNT
};

extern AnalogInputDev AnalogInputDevArray[static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)];


// =============================================================================
// DIGITAL DEVICE ENUMS AND DESCRIPTOR ARRAYS — EMPTY (COUNT = 0)
// =============================================================================

enum class DigitalInputDevID : uint8_t {
  SQUARE_BTN = 0,
  CROSS_BTN,
  CIRCLE_BTN,
  TRIANGLE_BTN,
  L1_BTN,
  R1_BTN,
  L2_BTN,
  R2_BTN,
  UP_ARROW,
  RIGHT_ARROW,
  DOWN_ARROW,
  LEFT_ARROW,
  SHARE_BTN,
  OPTIONS_BTN,
  PS_BTN,
  TOUCHPAD_BTN,
  L_STICK_BTN,
  R_STICK_BTN,
  DIGITAL_DEV_COUNT
};

extern DigitalInputDev digitalInputDevArray[static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)];



// =============================================================================
// TOP-LEVEL DESCRIPTOR — NO DEVICE
// =============================================================================

inline constexpr InputDev inputDev {
  .infoName = "PS4 dualshock controller",                                             // remote short description
  .protocol = RemoteProtocol::PS4_BLUETOOTH,                                          // Remote protocol definition
  .analogInputDev = AnalogInputDevArray,                                              // pointer to external AnalogInputDev structure
  .digitalInputDev = digitalInputDevArray,                                            // pointer to external DigitalgDev structure
  .analogInputDevCount = static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT),    // number of input analog channel
  .digitalInputDevCount = static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)  // number of input digital channel
};


// =============================================================================
// Hardware cap checks (PS4 DualShock 4 physical limits)
// =============================================================================

  // PS4 DS4 hardware: 6 analog axes (LX, LY, RX, RY, L2, R2), 18 digital buttons.
static_assert(static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)  <= 6u,
              "PS4_dualshock: ANALOG_DEV_COUNT exceeds PS4 DS4 analog axis count (6)");
static_assert(static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT) <= 18u,
              "PS4_dualshock: DIGITAL_DEV_COUNT exceeds PS4 DS4 button count (18)");

// EOF PS4_dualshock4.h
