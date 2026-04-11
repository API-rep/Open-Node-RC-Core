/******************************************************************************
 * @file dac_output.h
 * Platform router — include the correct register-level DAC output for the
 * target chip.
 *
 * @details Each platform header must provide the same inline function:
 *   `static inline void IRAM_ATTR dac_output(SoundHalCh ch, uint8_t sample)`
 *
 *   Add a new `#elif` branch when porting to another chip family.
 *****************************************************************************/
#pragma once

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
	#include "dac_output_esp32.h"

// #elif defined(ESP32S2) || defined(ARDUINO_ARCH_ESP32S2)
//   #include "dac_output_esp32s2.h"

#else
	#error "dac_output.h: no DAC output implementation for this target — add a new #elif branch."
#endif

// EOF dac_output.h
