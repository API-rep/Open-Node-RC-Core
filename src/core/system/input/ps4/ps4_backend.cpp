/******************************************************************************
 * @file  ps4_backend.cpp
 * @brief Backend of the PS4 DualShock source: the controller, over Bluetooth.
 *****************************************************************************/

#if defined(HAS_REMOTE_PS4)

#include "ps4_backend.h"

#include <PS4Controller.h>   // DualShock 4 over Bluetooth: the PS4 object

#include <core/system/combus/combus_access.h>   // combus_set_raw_analog(), combus_set_digital()
#include <core/system/combus/combus_res.h>      // CbusNeutral
#include <core/system/debug/logging/debug.h>



// =============================================================================
// 1. THE CONTROLLER
// =============================================================================

	/// One axis: its channel, how to read it, how to scale it.
struct Ps4Axis {
	AnalogComBusID      ch;
	int16_t             (*read)();
	const Ps4AxisScale& scale;
};

	/// One button: its channel, how to read it.
struct Ps4Button {
	DigitalComBusID ch;
	bool            (*read)();
};


static const Ps4Axis kAxes[] = {
	{ AnalogComBusID::PS4_LX, [] { return static_cast<int16_t>(PS4.LStickX()); }, kPs4Stick   },
	{ AnalogComBusID::PS4_LY, [] { return static_cast<int16_t>(PS4.LStickY()); }, kPs4Stick   },
	{ AnalogComBusID::PS4_RX, [] { return static_cast<int16_t>(PS4.RStickX()); }, kPs4Stick   },
	{ AnalogComBusID::PS4_RY, [] { return static_cast<int16_t>(PS4.RStickY()); }, kPs4Stick   },
	{ AnalogComBusID::PS4_L2, [] { return static_cast<int16_t>(PS4.L2Value()); }, kPs4Trigger },
	{ AnalogComBusID::PS4_R2, [] { return static_cast<int16_t>(PS4.R2Value()); }, kPs4Trigger },
};

static const Ps4Button kButtons[] = {
	{ DigitalComBusID::PS4_SQUARE,     [] { return static_cast<bool>(PS4.Square());   } },
	{ DigitalComBusID::PS4_CROSS,      [] { return static_cast<bool>(PS4.Cross());    } },
	{ DigitalComBusID::PS4_CIRCLE,     [] { return static_cast<bool>(PS4.Circle());   } },
	{ DigitalComBusID::PS4_TRIANGLE,   [] { return static_cast<bool>(PS4.Triangle()); } },
	{ DigitalComBusID::PS4_L1,         [] { return static_cast<bool>(PS4.L1());       } },
	{ DigitalComBusID::PS4_R1,         [] { return static_cast<bool>(PS4.R1());       } },
	{ DigitalComBusID::PS4_L2_PRESSED, [] { return static_cast<bool>(PS4.L2());       } },
	{ DigitalComBusID::PS4_R2_PRESSED, [] { return static_cast<bool>(PS4.R2());       } },
	{ DigitalComBusID::PS4_UP,         [] { return static_cast<bool>(PS4.Up());       } },
	{ DigitalComBusID::PS4_RIGHT,      [] { return static_cast<bool>(PS4.Right());    } },
	{ DigitalComBusID::PS4_DOWN,       [] { return static_cast<bool>(PS4.Down());     } },
	{ DigitalComBusID::PS4_LEFT,       [] { return static_cast<bool>(PS4.Left());     } },
	{ DigitalComBusID::PS4_SHARE,      [] { return static_cast<bool>(PS4.Share());    } },
	{ DigitalComBusID::PS4_OPTIONS,    [] { return static_cast<bool>(PS4.Options());  } },
	{ DigitalComBusID::PS4_PS,         [] { return static_cast<bool>(PS4.PSButton()); } },
	{ DigitalComBusID::PS4_TOUCHPAD,   [] { return static_cast<bool>(PS4.Touchpad()); } },
	{ DigitalComBusID::PS4_L3,         [] { return static_cast<bool>(PS4.L3());       } },
	{ DigitalComBusID::PS4_R3,         [] { return static_cast<bool>(PS4.R3());       } },
};



// =============================================================================
// 2. SCALING
// =============================================================================

/**
 * @brief Scale of one axis. Deadband first, then a linear map to the range of the bus.
 *
 * @details In order:
 *   1. Clamp the read to the range of the axis.
 *   2. Deadband: around the centre for a stick, above the rest for a trigger; inside
 *      it, the axis is at rest.
 *   3. Linear map: a stick to [0..busMax], a trigger to [neutral..busMax], so that the
 *      rest of every axis is the neutral, its safe value.
 */

uint16_t ps4_scale_axis( int16_t raw, const Ps4AxisScale& scale, uint32_t busMax )
{
		// 1. Clamp
	int32_t v = raw;

	if (v < scale.min) { v = scale.min; }
	if (v > scale.max) { v = scale.max; }

	const int32_t span = static_cast<int32_t>(scale.max) - scale.min;

		// 2. Deadband, then 3. map: a trigger
	if (scale.trigger) {
		if (v <= scale.min + scale.deadband) { v = scale.min; }

		return static_cast<uint16_t>(CbusNeutral + (v - scale.min) * (static_cast<int32_t>(busMax) - CbusNeutral) / span);
	}

		// 2. Deadband, then 3. map: a stick
	const int32_t centre = (static_cast<int32_t>(scale.min) + scale.max) / 2;

	if (v >= centre - scale.deadband && v <= centre + scale.deadband) { v = centre; }

	return static_cast<uint16_t>((v - scale.min) * static_cast<int32_t>(busMax) / span);
}



// =============================================================================
// 3. BACKEND
// =============================================================================

	/// The library keeps the address; the controller pairs to it.
void ps4_backend_begin( void* ctx )
{
	const char* address = static_cast<const char*>(ctx);

	if (address) {
		PS4.begin(address);
	} else {
		PS4.begin();
	}

	sys_log_info("[PS4] Bluetooth started, waiting for the controller (%s)\n", address ? address : "stored address");
}


/**
 * @brief Read of the controller. Every channel rewritten, on every call.
 *
 * @details Not connected: every channel at its safe value (neutral, released), and
 *   false.  Connected: each axis read and scaled, each button read.
 */

bool ps4_backend_refresh( ComBus& bus, void* /*ctx*/ )
{
	const bool connected = PS4.isConnected();

		// Each axis
	for (const Ps4Axis& a : kAxes) {
		const uint16_t v = connected ? ps4_scale_axis(a.read(), a.scale, bus.analogBusMaxVal)
		                             : bus.analogBus[static_cast<uint8_t>(a.ch)].safe;
		combus_set_raw_analog(bus, a.ch, v);
	}

		// Each button
	for (const Ps4Button& b : kButtons) {
		const bool v = connected ? b.read() : bus.digitalBus[static_cast<uint8_t>(b.ch)].safe;
		combus_set_digital(bus, b.ch, v);
	}

	return connected;
}

#endif  // HAS_REMOTE_PS4

// EOF ps4_backend.cpp
