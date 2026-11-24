/******************************************************************************
 * @file  ps4_backend.h
 * @brief Backend of the PS4 DualShock source: the controller, over Bluetooth.
 *
 * @details Thin on purpose: Bluetooth is bound to leave (one radio per config).  On
 *   every read, writes all the channels of the source (ps4.cb), scaled to the bus:
 *   - a stick: [min..max] of the controller to [0..max], neutral at the centre, with a
 *     deadband around the centre;
 *   - an analog trigger: [min..max] to [neutral..max], neutral when released, with a
 *     deadband above the rest;
 *   - a button: as read.
 *   Not connected, every channel at its safe value.  The module (remote_module.h)
 *   takes ps4_backend_begin() and ps4_backend_refresh() as its RemoteBackend.
 *****************************************************************************/
#pragma once

#if defined(HAS_REMOTE_PS4)

#if !defined(HAS_REMOTE)
#error "HAS_REMOTE_PS4 needs HAS_REMOTE: a command source writes the inputs of the board that acquires the commands"
#endif

#include <stdint.h>
#include <stdbool.h>

#include <core/config/machines/combus_types.h>   // ComBus
#include <core/system/input/remote_module.h>     // RemoteBackend



// =============================================================================
// 1. SCALING
// =============================================================================

/**
 * @brief How one axis of the controller is scaled to the bus.
 */

struct Ps4AxisScale {
	int16_t min;        ///< value of the controller at one end (rest, for a trigger)
	int16_t max;        ///< value of the controller at the other end
	uint8_t deadband;   ///< in units of the controller; 0 = none
	bool    trigger;    ///< analog trigger: rest at min, scaled to [neutral..max]
};

	/// Sticks of the DualShock: -127..127; triggers: 0..255, a deadband of 5 (idle noise).
inline constexpr Ps4AxisScale kPs4Stick   = { -127, 127, 0u, false };
inline constexpr Ps4AxisScale kPs4Trigger = {    0, 255, 5u, true  };


/**
 * @brief Scale one read of an axis to the bus.
 *
 * @param raw     Value read on the controller.
 * @param scale   Scaling of the axis.
 * @param busMax  Largest analog value of the bus.
 *
 * @return The value of the channel of the source.
 */

uint16_t ps4_scale_axis( int16_t raw, const Ps4AxisScale& scale, uint32_t busMax );



// =============================================================================
// 2. BACKEND
// =============================================================================

/**
 * @brief Start the Bluetooth stack and wait for the controller.
 *
 * @param ctx  The Bluetooth address of the controller (const char*); nullptr = the one
 *   stored by the controller library.
 */

void ps4_backend_begin( void* ctx );


/**
 * @brief Read the controller, and write every channel of the source.
 *
 * @param bus  ComBus the channels are written to.
 * @param ctx  Unused.
 *
 * @return true if the controller is connected.
 */

bool ps4_backend_refresh( ComBus& bus, void* ctx );

#endif  // HAS_REMOTE_PS4

// EOF ps4_backend.h
