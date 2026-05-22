#include "PS4_dualshock_map.h"

  // input to combus analog channel mapping
const InputAnalogMap InputAnalogMapArray[] = {
  // { Index manette, Canal ComBus, Inversion }
  { AnalogInputDevID::LY_STICK,  AnalogComBusID::THROTTLE_BUS,   false },
  { AnalogInputDevID::LX_STICK,  AnalogComBusID::STEERING_BUS,   false },
  { AnalogInputDevID::RY_STICK,  AnalogComBusID::DUMP_BUS,       false },
  { AnalogInputDevID::L2_BUTTON, AnalogComBusID::BRAKE_BUS,      false }
};

  // number of analog mappings in InputAnalogMap array
const uint8_t InputAnalogMapCount = sizeof(InputAnalogMapArray) / sizeof(InputAnalogMap);



  // input to combus analog channel mapping
const InputDigitalMap InputDigitalMapArray[] = {
  { DigitalInputDevID::CIRCLE_BTN,   DigitalComBusID::HORN,         false },
  { DigitalInputDevID::TRIANGLE_BTN, DigitalComBusID::KEY,          false },
  { DigitalInputDevID::OPTIONS_BTN,  DigitalComBusID::DIRECT_DRIVE_BTN, false },  // raw button — toggle logic in main.cpp derives DIRECT_DRIVE state
  { DigitalInputDevID::SHARE_BTN,    DigitalComBusID::SUBGEAR_SET,  false },  // crawler mode toggle
  { DigitalInputDevID::UP_ARROW,     DigitalComBusID::SUBGEAR_UP,   false },  // crawler: faster sub-gear
  { DigitalInputDevID::DOWN_ARROW,   DigitalComBusID::SUBGEAR_DOWN, false }   // crawler: slower sub-gear
};

  // number of digital mappings in InputDigital Map array
const uint8_t InputDigitalMapCount = sizeof(InputDigitalMapArray) / sizeof(InputDigitalMap);
