/**
 * @file combus_manager.cpp
 * @brief ComBus utility helpers implementation
 */

#include "combus_manager.h"

#include <Arduino.h>
#include <core/config/machines/combus_types.h>


/**
 * @brief Reset ComBus drive flags according to active input mapping
 */

void resetComBusDriveFlags(ComBus &bus) {
	for (uint8_t i = 0; i < InputAnalogMapCount; i++) {
		uint8_t ch = static_cast<uint8_t>(InputAnalogMapArray[i].busChannel);
		bus.analogBus[ch].isDrived = false;
	}

	for (uint8_t i = 0; i < InputDigitalMapCount; i++) {
		uint8_t ch = static_cast<uint8_t>(InputDigitalMapArray[i].busChannel);
		bus.digitalBus[ch].isDrived = false;
	}
}


void combus_watchdog(ComBus &bus, uint32_t timeoutMs) {
	if ((millis() - bus.lastFrameMs) >= timeoutMs) {
		resetComBusDriveFlags(bus);
	}
}

// EOF combus_manager.cpp
