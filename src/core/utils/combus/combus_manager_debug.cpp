/**
 * @file combus_manager_debug.cpp
 * @brief ComBus debug helpers
 */

#include "combus_manager.h"

#include <core/config/combus/combus.h>


#if defined(DEBUG_COMBUS) || defined(DEBUG_ALL)

/**
 * @brief Compact ComBus runtime snapshot
 */

void debugComBusSnapshot(ComBus &bus) {
	Serial.println(F("[COMBUS] Snapshot"));
	Serial.printf("  RunLevel: %d\n", static_cast<int>(bus.runLevel));

	Serial.println(F("  Analog:"));
	for (uint8_t i = 0; i < InputAnalogMapCount; i++) {
		const InputAnalogMap m = InputAnalogMapArray[i];
		uint8_t ch = static_cast<uint8_t>(m.busChannel);
		const char* chName = bus.analogBus[ch].infoName ? bus.analogBus[ch].infoName : "AN_CH";

		Serial.printf("    [%02d] %-12s v=%5u drv=%d\n",
		              ch,
		              chName,
		              bus.analogBus[ch].value,
		              bus.analogBus[ch].isDrived ? 1 : 0);
	}

	Serial.println(F("  Digital:"));
	for (uint8_t i = 0; i < InputDigitalMapCount; i++) {
		const InputDigitalMap m = InputDigitalMapArray[i];
		uint8_t ch = static_cast<uint8_t>(m.busChannel);
		const char* chName = bus.digitalBus[ch].infoName ? bus.digitalBus[ch].infoName : "DG_CH";

		Serial.printf("    [%02d] %-12s st=%d drv=%d\n",
		              ch,
		              chName,
		              bus.digitalBus[ch].value ? 1 : 0,
		              bus.digitalBus[ch].isDrived ? 1 : 0);
	}
}

#endif

// EOF combus_manager_debug.cpp
