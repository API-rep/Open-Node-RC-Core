/******************************************************************************
 * @file  ps4_map.cpp
 * @brief Mapper of the PS4 source for a dumper truck: the table of the type.
 *
 * @details The rows of the former PS4_dualshock_map.cpp, input by input.  L2 went to
 *   BRAKE_BUS, an output of the machine: it goes to the input BRAKE_STICK now, which the
 *   INPUT_BRAKE chain rescales to BRAKE_BUS (WIP - Local remote §10).
 *****************************************************************************/

#if defined(HAS_REMOTE_PS4) && defined(MACHINE_TYPE_DUMPER_TRUCK)

#include "ps4_map.h"

#include <iterator>   // std::size



// =============================================================================
// 1. ROWS
// =============================================================================

static const RemoteAnalogRoute kAnalog[] = {
	// { channel of the source,  input of the type,               inverted }
	{ AnalogComBusID::PS4_LY,    AnalogComBusID::THROTTLE_STICK,  false },
	{ AnalogComBusID::PS4_LX,    AnalogComBusID::STEERING_STICK,  false },
	{ AnalogComBusID::PS4_RY,    AnalogComBusID::DUMP_STICK,      false },
	{ AnalogComBusID::PS4_L2,    AnalogComBusID::BRAKE_STICK,     false },
};

static const RemoteDigitalRoute kDigital[] = {
	// { channel of the source,     input of the type,                    inverted }
	{ DigitalComBusID::PS4_CIRCLE,   DigitalComBusID::HORN_BTN,           false },
	{ DigitalComBusID::PS4_TRIANGLE, DigitalComBusID::KEY_BTN,            false },
	{ DigitalComBusID::PS4_OPTIONS,  DigitalComBusID::DIRECT_DRIVE_BTN,   false },
	{ DigitalComBusID::PS4_SHARE,    DigitalComBusID::SUBGEAR_SET_BTN,    false },
	{ DigitalComBusID::PS4_UP,       DigitalComBusID::GEAR_UP_BTN,        false },
	{ DigitalComBusID::PS4_DOWN,     DigitalComBusID::GEAR_DOWN_BTN,      false },
	{ DigitalComBusID::PS4_SQUARE,   DigitalComBusID::CRUISE_TOGGLE_BTN,  false },
	{ DigitalComBusID::PS4_L3,       DigitalComBusID::CRUISE_UPDATE_BTN,  false },
};



// =============================================================================
// 2. TABLES
// =============================================================================

const RemoteMapTable kPs4TypeMap = {
	.name    = "dumper_truck/ps4",
	.analog  = { kAnalog,  static_cast<uint8_t>(std::size(kAnalog))  },
	.digital = { kDigital, static_cast<uint8_t>(std::size(kDigital)) },
};


	/// No table of the instance unless an instance defines one.
__attribute__((weak)) const RemoteMapTable* ps4_instance_map( void )
{
	return nullptr;
}

#endif  // HAS_REMOTE_PS4 && MACHINE_TYPE_DUMPER_TRUCK

// EOF ps4_map.cpp
