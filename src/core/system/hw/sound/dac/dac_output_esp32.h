/******************************************************************************
 * @file dac_output_esp32.h
 * Register-level DAC sample output — ESP32 internal DAC implementation.
 *
 * @details Provides a single inline function that writes one 8-bit sample
 *   to the hardware DAC register for a given channel.  Called exclusively
 *   from ISR context (IRAM_ATTR) — must stay free of any blocking call.
 *
 *   Platform selection: if a future chip needs different register access,
 *   add a new section guarded by `CONFIG_IDF_TARGET_*` and keep the same
 *   `dac_output()` signature.
 *****************************************************************************/
#pragma once

#include "../sound_hal_audio.h"                // SoundHalCh, kChEngine
#include <soc/rtc_io_reg.h>                    // RTC_IO_PAD_DACx_REG
#include <Arduino.h>                           // IRAM_ATTR


// =============================================================================
// 1. DAC REGISTER WRITE
// =============================================================================

/**
 * @brief Write one sample to the hardware DAC register for the given channel.
 *
 * @details Inlined into the caller (ISR) — zero function-call overhead.
 *   The `if` branch is eliminated at compile time when `ch` is a constant
 *   (which it always is in each ISR).
 *
 *   ESP32 internal DAC:
 *   - kChEngine  → DAC1 / GPIO25 (RTC_IO_PAD_DAC1_REG)
 *   - kChEffects → DAC2 / GPIO26 (RTC_IO_PAD_DAC2_REG)
 */
static inline void IRAM_ATTR dac_output(SoundHalCh ch, uint8_t sample)
{
	if (ch == kChEngine)
		SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, sample, RTC_IO_PDAC1_DAC_S);
	else
		SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, sample, RTC_IO_PDAC2_DAC_S);
}

// EOF dac_output_esp32.h
