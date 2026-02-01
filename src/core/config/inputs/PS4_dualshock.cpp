#include "PS4_dualshock.h"

AnalogInputDev AnalogInputDevArray[static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)] = {

  {
    .infoName = "left X axis stick",      // device short description
    .type = RemoteComp::ANALOG_STICK,     // analog device type
    .minVal = DEF_STICK_MIN_VAL,          // minimum value return by analog device decoder module at lower state
    .maxVal = DEF_STICK_MAX_VAL,          // maximum value return by analog device decoder module at higher state
    .isInverted = false                   // true if analog device axe is inverted
  },

  {
    .infoName = "left Y axis stick",      // device short description
    .type = RemoteComp::ANALOG_STICK,     // analog device type
    .minVal = DEF_STICK_MIN_VAL,          // minimum value return by analog device
    .maxVal = DEF_STICK_MAX_VAL,          // maximum value return by analog device decoder module at higher state
    .isInverted = false                   // true if analog device axe is inverted
  },
  
  {.infoName = "right X axis stick",      // device short description
    .type = RemoteComp::ANALOG_STICK,     // analog device type
    .minVal = DEF_STICK_MIN_VAL,          // minimum value return by analog device decoder module at lower state
    .maxVal = DEF_STICK_MAX_VAL,          // maximum value return by analog device decoder module at higher state
    .isInverted = false                   // true if analog device axe is inverted
  },
  
  {.infoName = "right Y axis stick",      // device short description
    .type = RemoteComp::ANALOG_STICK,     // analog device type
    .minVal = DEF_STICK_MIN_VAL,          // minimum value return by analog device decoder module at lower state
    .maxVal = DEF_STICK_MAX_VAL,          // maximum value return by analog device
    .isInverted = false                   // true if analog device axe is inverted
  },
  
  {.infoName = "L2 analog button",         // device short description  
    .type = RemoteComp::ANALOG_BUTTON,     // analog device type
    .minVal = DEF_ANALOG_BUTTON_MIN_VAL,   // minimum value return by analog device decoder module at lower state
    .maxVal = DEF_ANALOG_BUTTON_MAX_VAL,   // maximum value return by analog device decoder module at higher state
    .isInverted = false                    // true if analog device axe is inverted
  },
  
  {.infoName = "R2 analog button",         // device short description
    .type = RemoteComp::ANALOG_BUTTON,     // analog device type
    .minVal = DEF_ANALOG_BUTTON_MIN_VAL,   // minimum value return by analog device decoder module at lower state
    .maxVal = DEF_ANALOG_BUTTON_MAX_VAL,   // maximum value return by analog device decoder module at higher state
    .isInverted = false                    // true if analog device axe is inverted
  }
};



DigitalInputDev digitalInputDevArray[static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)] = {

  { .infoName = "Square Button",   .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // SQUARE_BTN
  { .infoName = "Cross Button",    .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // CROSS_BTN
  { .infoName = "Circle Button",   .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // CIRCLE_BTN
  { .infoName = "Triangle Button", .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // TRIANGLE_BTN
  
  { .infoName = "L1 Button",       .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // L1_BTN
  { .infoName = "R1 Button",       .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // R1_BTN
  { .infoName = "L2 Button",       .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // L2_BTN
  { .infoName = "R2 Button",       .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // R2_BTN

  { .infoName = "Up Arrow",        .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // UP_ARROW
  { .infoName = "Right Arrow",     .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // RIGHT_ARROW
  { .infoName = "Down Arrow",      .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // DOWN_ARROW
  { .infoName = "Left Arrow",      .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // LEFT_ARROW

  { .infoName = "Share Button",    .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // SHARE_BTN
  { .infoName = "Options Button",  .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // OPTIONS_BTN
  { .infoName = "PS Button",       .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // PS_BTN
  { .infoName = "Touchpad Button", .type = RemoteComp::PUSH_BUTTON, .isInverted = false },  // TOUCHPAD_BTN

  { .infoName = "Left Stick Btn",  .type = RemoteComp::PUSH_BUTTON, .isInverted = false }, // 16 (L3)
  { .infoName = "Right Stick Btn", .type = RemoteComp::PUSH_BUTTON, .isInverted = false }  // 17 (R3)
};