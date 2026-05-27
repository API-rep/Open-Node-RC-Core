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
  { DigitalInputDevID::CIRCLE_BTN,   DigitalComBusID::HORN_BTN,         false },
  { DigitalInputDevID::TRIANGLE_BTN, DigitalComBusID::KEY_BTN,          false },
  { DigitalInputDevID::OPTIONS_BTN,  DigitalComBusID::DIRECT_DRIVE_BTN, false },  // raw button — ctrl chain derives DIRECT_DRIVE state
  { DigitalInputDevID::SHARE_BTN,    DigitalComBusID::SUBGEAR_SET_BTN,  false },  // crawler mode toggle
  { DigitalInputDevID::UP_ARROW,     DigitalComBusID::GEAR_UP_BTN,   false },  // gear up (manual mode) / subgear faster (crawl mode)
  { DigitalInputDevID::DOWN_ARROW,   DigitalComBusID::GEAR_DOWN_BTN, false }   // gear down (manual mode) / subgear slower (crawl mode)
};

  // number of digital mappings in InputDigital Map array
const uint8_t InputDigitalMapCount = sizeof(InputDigitalMapArray) / sizeof(InputDigitalMap);
