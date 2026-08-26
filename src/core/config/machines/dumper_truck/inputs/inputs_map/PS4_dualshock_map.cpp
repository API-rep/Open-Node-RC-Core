/*!****************************************************************************
 * @file  PS4_dualshock_map.cpp
 * @brief Default input-device → ComBus mapping for dumper_truck + PS4 DualShock.
 *
 * @details A9.9: migrated from machines/.../volvo_A60H_bruder/inputs_map/ to
 *   core/config/machines/dumper_truck/inputs/inputs_map/ so the default applies
 *   to ALL dumper_truck instances (Volvo A60H, Cat 770, Bell B45, Komatsu HD785, ...).
 *
 *   The mapping is INDEX-ONLY — no scaling, no inversion logic. Each entry
 *   is `{ device, busChannel, isInverted }`. The actual mapping to ComBus
 *   units (uint16_t [0..65535]) is performed by input_update() via map() over
 *   the device's AnalogInputDev descriptor (min/max from PS4_dualshock.cpp).
 *
 *   Per-instance override: future iteration.
 *******************************************************************************/

#include "PS4_dualshock_map.h"

// This TU is one half of the input-mapping dispatch. The empty mapping
// (`nop_map.cpp`) defines the same four symbols when the INPUT_NONE
// flag is set. Compiling both TUs together yields a linker
// `multiple definition` error on those four symbols.
//
// The matching guard is in `nop_map.cpp` (complementary
// #if defined(INPUT_NONE)). PlatformIO compiles every .cpp in the
// active build_src_filter, so the guard has to be in the .cpp itself,
// not just in a .h (which would only suppress re-inclusion of the
// declarations). This mirrors the `inputs.h` dispatch at the header
// level.
#if defined(INPUT_PS4_DS4_BT)

  // input → ComBus analog channel mapping
const InputAnalogMap InputAnalogMapArray[] = {
  // { Index manette,                    Canal ComBus,                  Inversion }
  { AnalogInputDevID::LY_STICK,         AnalogComBusID::THROTTLE_STICK, false },  // raw L-stick Y → throttle (conditioned by INPUT_THROTTLE)
  { AnalogInputDevID::LX_STICK,         AnalogComBusID::STEERING_STICK, false },  // raw L-stick X → steering (conditioned by SIM)
  { AnalogInputDevID::RY_STICK,         AnalogComBusID::DUMP_STICK,     false },  // raw R-stick Y → dump (conditioned by SIM)
  { AnalogInputDevID::L2_BUTTON,        AnalogComBusID::BRAKE_BUS,      false }   // raw L2 trigger → brake
};

  // number of analog mappings in InputAnalogMap array
const uint8_t InputAnalogMapCount = sizeof(InputAnalogMapArray) / sizeof(InputAnalogMap);



  // input → ComBus digital channel mapping
const InputDigitalMap InputDigitalMapArray[] = {
  { DigitalInputDevID::CIRCLE_BTN,      DigitalComBusID::HORN_BTN,            false },  // raw CIRCLE  → horn
  { DigitalInputDevID::TRIANGLE_BTN,    DigitalComBusID::KEY_BTN,             false },  // raw TRIANGLE → ignition key
  { DigitalInputDevID::OPTIONS_BTN,     DigitalComBusID::DIRECT_DRIVE_BTN,    false },  // raw OPTIONS → direct-drive toggle (INPUT_DIRECT_DRIVE derives state)
  { DigitalInputDevID::SHARE_BTN,       DigitalComBusID::SUBGEAR_SET_BTN,     false },  // raw SHARE   → crawler mode toggle
  { DigitalInputDevID::UP_ARROW,        DigitalComBusID::GEAR_UP_BTN,         false },  // raw UP     → gear up / subgear faster
  { DigitalInputDevID::DOWN_ARROW,      DigitalComBusID::GEAR_DOWN_BTN,       false },  // raw DOWN   → gear down / subgear slower
  { DigitalInputDevID::SQUARE_BTN,      DigitalComBusID::CRUISE_TOGGLE_BTN,   false },  // raw SQUARE → cruise toggle
  { DigitalInputDevID::L_STICK_BTN,     DigitalComBusID::CRUISE_UPDATE_BTN,   false },  // raw L3     → cruise update held speed
};

  // number of digital mappings in InputDigitalMap array
const uint8_t InputDigitalMapCount = sizeof(InputDigitalMapArray) / sizeof(InputDigitalMap);

#endif  // INPUT_PS4_DS4_BT


// EOF PS4_dualshock_map.cpp
