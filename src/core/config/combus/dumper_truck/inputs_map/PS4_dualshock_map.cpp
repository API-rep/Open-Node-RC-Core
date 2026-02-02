#include "PS4_dualshock_map.h"

  // input to combus analog channel mapping
const InputAnalogMap InputAnalogMapArray[] = {
  // { Index manette, Canal ComBus, Inversion }
  { AnalogInputDevID::LY_STICK,  AnalogComBusID::DRIVE_SPEED_BUS, false  }, // Souvent inversé sur les sticks
  { AnalogInputDevID::LX_STICK,  AnalogComBusID::STEERING_BUS,    false },
  { AnalogInputDevID::RY_STICK,  AnalogComBusID::DUMP_BUS,        false }
};

  // number of analog mappings in InputAnalogMap array
const uint8_t InputAnalogMapCount = sizeof(InputAnalogMapArray) / sizeof(InputAnalogMap);



  // input to combus analog channel mapping
const InputDigitalMap InputDigitalMapArray[] = {
  { DigitalInputDevID::CIRCLE_BTN,   DigitalComBusID::HORN,   false },
  { DigitalInputDevID::TRIANGLE_BTN, DigitalComBusID::LIGHTS, false }
};

  // number of digital mappings in InputDigital Map array
const uint8_t InputDigitalMapCount = sizeof(InputDigitalMapArray) / sizeof(InputDigitalMap);