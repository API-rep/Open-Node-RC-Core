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

#include <config/config.h>
#include "const.h"

#include <PS4Controller.h>

#define REMOTE_INFO_NAME  "PS4 dualshock 4 Bluetooth controller"

#define REMOTE_ID          "PS4_DS4_BT"
#define REMOTE_PROTOCOL     PS4_BLUETOOTH


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

  // analog channel 1 device - Left X axis stick
#define REMOTE_A_CH_1
  #ifdef REMOTE_A_CH_1
    #define REMOTE_A_CH_1_INFO_NAME    "left X axis stick"         // remote stick short description
    #define REMOTE_A_CH_1_DEV_TYPE     ANALOG_STICK                // remote device type
    #define REMOTE_A_CH_1_MIN_VAL      DEF_STICK_MIN_VAL           // minimum value return by stick decoder module at lower state
    #define REMOTE_A_CH_1_MAX_VAL      DEF_STICK_MAX_VAL           // maximum value return by stick decoder module at higher state
    #define REMOTE_A_CH_1_INVERTED     false                       // true if stick axe is inverted
  #endif

  // analog channel 2 device - Left Y axis stick
#define REMOTE_A_CH_2
  #ifdef REMOTE_A_CH_2
    #define REMOTE_A_CH_2_INFO_NAME    "left Y axis stick"         // remote stick short description
    #define REMOTE_A_CH_2_DEV_TYPE     ANALOG_STICK                // remote device type
    #define REMOTE_A_CH_2_MIN_VAL      DEF_STICK_MIN_VAL           // minimum value return by stick decoder module at lower state
    #define REMOTE_A_CH_2_MAX_VAL      DEF_STICK_MAX_VAL           // maximum value return by stick decoder module at higher state
    #define REMOTE_A_CH_2_INVERTED     false                       // true if stick axe is inverted
  #endif

  // analog channel 3 device - Right X axis stick
#define REMOTE_A_CH_3
  #ifdef REMOTE_A_CH_3
    #define REMOTE_A_CH_3_INFO_NAME    "right X axis stick"        // remote stick short description
    #define REMOTE_A_CH_3_DEV_TYPE     ANALOG_STICK                // remote device type
    #define REMOTE_A_CH_3_MIN_VAL      DEF_STICK_MIN_VAL           // minimum value return by stick decoder module at lower state
    #define REMOTE_A_CH_3_MAX_VAL      DEF_STICK_MAX_VAL           // maximum value return by stick decoder module at higher state
    #define REMOTE_A_CH_3_INVERTED     false                       // true if stick axe is inverted
  #endif

    // analog channel 4 device - Right Y axis stick
#define REMOTE_A_CH_4
  #ifdef REMOTE_A_CH_4
    #define REMOTE_A_CH_4_INFO_NAME    "right Y axis stick"        // remote stick short description
    #define REMOTE_A_CH_4_DEV_TYPE     ANALOG_STICK                // remote device type
    #define REMOTE_A_CH_4_MIN_VAL      DEF_STICK_MIN_VAL           // minimum value return by stick decoder module at lower state
    #define REMOTE_A_CH_4_MAX_VAL      DEF_STICK_MAX_VAL           // maximum value return by stick decoder module at higher state
    #define REMOTE_A_CH_4_INVERTED     false                       // true if stick axe is inverted
  #endif

    // analog channel 5 device - L2 analog button
#define REMOTE_A_CH_5
  #ifdef REMOTE_A_CH_5
    #define REMOTE_A_CH_5_INFO_NAME    "L2 analog button"          // remote stick short description
    #define REMOTE_A_CH_5_DEV_TYPE     ANALOG_BUTTON               // remote device type
    #define REMOTE_A_CH_5_MIN_VAL      DEF_ANALOG_BUTTON_MIN_VAL   // minimum value return by stick decoder module at lower state
    #define REMOTE_A_CH_5_MAX_VAL      DEF_ANALOG_BUTTON_MAX_VAL   // maximum value return by stick decoder module at higher state
    #define REMOTE_A_CH_5_INVERTED     false                       // true if stick axe is inverted
  #endif

    // analog channel 6 device - R2 analog button
#define REMOTE_A_CH_6
  #ifdef REMOTE_A_CH_6
    #define REMOTE_A_CH_6_INFO_NAME    "R2 analog button"          // remote stick short description
    #define REMOTE_A_CH_6_DEV_TYPE     ANALOG_BUTTON               // remote device type
    #define REMOTE_A_CH_6_MIN_VAL      DEF_ANALOG_BUTTON_MIN_VAL   // minimum value return by stick decoder module at lower state
    #define REMOTE_A_CH_6_MAX_VAL      DEF_ANALOG_BUTTON_MAX_VAL   // maximum value return by stick decoder module at higher state
    #define REMOTE_A_CH_6_INVERTED     false                       // true if stick axe is inverted
  #endif

      // Remote switch 1 device - Square button
#define REMOTE_D_CH_1
  #ifdef REMOTE_D_CH_1
    #define REMOTE_D_CH_1_INFO_NAME   "square Button"              // remote switch short description
    #define REMOTE_D_CH_1_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_1_INVERTED    false                        // true if switch axe is inverted
  #endif

  // Remote switch 2 device - Cross button
#define REMOTE_D_CH_2
  #ifdef REMOTE_D_CH_2
    #define REMOTE_D_CH_2_INFO_NAME   "cross Button"               // remote switch short description
    #define REMOTE_D_CH_2_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_2_INVERTED    false                        // true if switch axe is inverted
  #endif

  // Remote switch 3 device - Circle button
#define REMOTE_D_CH_3
  #ifdef REMOTE_D_CH_3
    #define REMOTE_D_CH_3_INFO_NAME   "circle Button"              // remote switch short description
    #define REMOTE_D_CH_3_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_3_INVERTED    false                        // true if switch axe is inverted
  #endif

    // Remote switch 4 device - Triangle button
#define REMOTE_D_CH_4  // devient REMOTE_SW
  #ifdef REMOTE_D_CH_4
    #define REMOTE_D_CH_4_INFO_NAME   "triangle Button"            // remote switch short description
    #define REMOTE_D_CH_4_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_4_INVERTED    false                        // true if switch axe is inverted
  #endif

  // Remote switch 5 device - L1 button
#define REMOTE_D_CH_5
  #ifdef REMOTE_D_CH_5
    #define REMOTE_D_CH_5_INFO_NAME   "L1 Button"                  // remote switch short description
    #define REMOTE_D_CH_5_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_5_INVERTED    false                        // true if switch axe is inverted
  #endif  

  // Remote switch 6 device - R1 button
#define REMOTE_D_CH_6
  #ifdef REMOTE_D_CH_6
    #define REMOTE_D_CH_6_INFO_NAME   "R1 Button"                  // remote switch short description
    #define REMOTE_D_CH_6_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_6_INVERTED    false                        // true if switch axe is inverted
  #endif

  // Remote switch 7 device - L2 button (digital)
#define REMOTE_D_CH_7
  #ifdef REMOTE_D_CH_7
    #define REMOTE_D_CH_7_INFO_NAME   "L2 Button"                  // remote switch short description
    #define REMOTE_D_CH_7_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_7_INVERTED    false                        // true if switch axe is inverted
  #endif

  // Remote switch 8 device - R2 button (digital)
#define REMOTE_D_CH_8
  #ifdef REMOTE_D_CH_8
    #define REMOTE_D_CH_8_INFO_NAME   "L2 Button"                  // remote switch short description
    #define REMOTE_D_CH_8_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_8_INVERTED    false                        // true if switch axe is inverted
  #endif

  // Remote switch 9 device - left stick button
#define REMOTE_D_CH_9
  #ifdef REMOTE_D_CH_9
    #define REMOTE_D_CH_9_INFO_NAME   "left stick button"          // remote switch short description
    #define REMOTE_D_CH_9_DEV_TYPE    PUSH_BUTTON                  // remote device type
    #define REMOTE_D_CH_9_INVERTED    false                        // true if switch axe is inverted
  #endif

 // Remote switch 10 device - right stick button
#define REMOTE_D_CH_10
  #ifdef REMOTE_D_CH_10
    #define REMOTE_D_CH_10_INFO_NAME   "left stick button"         // remote switch short description
    #define REMOTE_D_CH_10_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_10_INVERTED    false                       // true if switch axe is inverted
  #endif

  // Remote switch 11 device - Share button
#define REMOTE_D_CH_11
  #ifdef REMOTE_D_CH_11
    #define REMOTE_D_CH_11_INFO_NAME   "share Button"              // remote switch short description
    #define REMOTE_D_CH_11_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_11_INVERTED    false                       // true if switch axe is inverted
  #endif

  // Remote switch 12 device - up arrow
#define REMOTE_D_CH_12
  #ifdef REMOTE_D_CH_12
    #define REMOTE_D_CH_12_INFO_NAME   "up arrow"                  // remote switch short description
    #define REMOTE_D_CH_12_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_12_INVERTED    false                       // true if switch axe is inverted
  #endif


  // Remote switch 13 device - Right arrow
#define REMOTE_D_CH_13
  #ifdef REMOTE_D_CH_13
    #define REMOTE_D_CH_13_INFO_NAME   "right arrow"               // remote switch short description
    #define REMOTE_D_CH_13_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_13_INVERTED    false                       // true if switch axe is inverted
  #endif

  // Remote switch 14 device - down arrow
#define REMOTE_D_CH_14
  #ifdef REMOTE_D_CH_14
    #define REMOTE_D_CH_14_INFO_NAME   "down arrow"                // remote switch short description
    #define REMOTE_D_CH_14_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_14_INVERTED    false                       // true if switch axe is inverted
  #endif

  // Remote switch 15 device - left arrow
#define REMOTE_D_CH_15
  #ifdef REMOTE_D_CH_15
    #define REMOTE_D_CH_15_INFO_NAME   "left arrow"                // remote switch short description
    #define REMOTE_D_CH_15_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_15_INVERTED    false                       // true if switch axe is inverted
  #endif

  // Remote switch 16 device - touchpad button
#define REMOTE_D_CH_16
  #ifdef REMOTE_D_CH_16
    #define REMOTE_D_CH_16_INFO_NAME   "touchpad button"           // remote switch short description
    #define REMOTE_D_CH_16_DEV_TYPE    PUSH_BUTTON                 // remote device type
    #define REMOTE_D_CH_16_INVERTED    false                       // true if switch axe is inverted
  #endif

//    // Remote switch 17 device - PS button
// #define REMOTE_D_CH_17
//   #ifdef REMOTE_D_CH_17
//     #define REMOTE_D_CH_17_INFO_NAME   "PS button"                 // remote switch short description
//     #define REMOTE_D_CH_17_DEV_TYPE    PUSH_BUTTON                 // remote device type
//     #define REMOTE_D_CH_17_INVERTED    false                       // true if switch axe is inverted
//   #endif
  
// EOF PS4_dualshock4.h