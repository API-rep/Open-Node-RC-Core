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

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include <config/config.h>

#include <PS4Controller.h>

/**
 * Device relative values
 */

#define DEF_STICK_MIN_VAL  -127          // maximum negative value for sticks
#define DEF_STICK_MAX_VAL   127          // maximum positive value for sticks

#define DEF_ANALOG_BUTTON_MIN_VAL    0   // maximum negative value for analog button
#define DEF_ANALOG_BUTTON_MAX_VAL  255   // maximum positive value for analog button

/**
 * Remote devices definition
  Place here all config of devices embedded in the remote suhc as:
 * - Analog axis (sticks, sliders, analog buttons ...)
 * - Switch type devices (switchs, buttons ...)
 */

/** @brief Remote analog config structure definition */

  // remote analog device index
enum AnalogDevID { LX_STICK = 0,
                   LY_STICK,
                   RX_STICK,
                   RY_STICK,
                   L2_BUTTON,
                   R2_BUTTON,
                   ANALOG_DEV_COUNT};

  // remote analog device config structure array
extern AnalogDev analogDevArray[ANALOG_DEV_COUNT];



/** @brief Remote digital config structure definition */

  // remote digital device index
enum DigitalDevID { SQUARE_BTN = 0,
                    CROSS_BTN,
                    CIRCLE_BTN,
                    TRIANGLE_BTN,
                    L1_BTN,
                    R1_BTN,
                    L2_BTN,
                    R2_BTN,
                    LEFT_STICK_BTN,
                    RIGHT_STICK_BTN,
                    SHARE_BTN,
                    UP_ARROW,
                    RIGHT_ARROW,
                    DOWN_ARROW,
                    LEFT_ARROW,
                    TOUCHPAD_BTN,
                    PS_BTN,
                    DIGITAL_DEV_COUNT};

  // remote digital device config structure array
extern DigitalDev digitalDevArray[ANALOG_DEV_COUNT];



/** @brief Remote config structure definition */
//  ADD TO STRUCT IF NEED     #define REMOTE_ID          "PS4_DS4_BT"

inline constexpr Remote remote {
  .infoName = "PS4 dualshock controller",     // remote short description
  .protocol = RemoteProtocol::PS4_BLUETOOTH,  // Remote protocol definition
  .analogDev = analogDevArray,                // pointer to external AnalogDev structure
  .digitalDev = digitalDevArray,              // pointer to external DigitalgDev structure
  .analogDevCount = ANALOG_DEV_COUNT,         // number of input analog channel
  .digitalDevCount = DIGITAL_DEV_COUNT        // number of input digital channel
};

// EOF PS4_dualshock4.h