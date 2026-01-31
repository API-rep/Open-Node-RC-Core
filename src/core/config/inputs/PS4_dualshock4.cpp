#include "PS4_dualshock4.h"

AnalogDev analogDevArray[ANALOG_DEV_COUNT] = {

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



DigitalDev digitalDevArray[DIGITAL_DEV_COUNT] = {

  {
    .infoName = "Square Button",        // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "Cross Button",         // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "Circle Button",        // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "Triangle Button",      // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "L1 Button",            // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "R1 Button",            // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "L2 Button",            // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "R2 Button",            // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },

  {
    .infoName = "left stick button",    // device short description
    .type = RemoteComp::PUSH_BUTTON,    // switch device type
    .isInverted = false                 // true if switch logic is inverted
  },
};