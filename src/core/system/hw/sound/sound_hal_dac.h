/******************************************************************************
 * @file sound_hal_dac.h
 * Sound module — ESP32 internal DAC audio backend.
 *
 * @details Implementation of `SoundHalAudio` using the two built-in 8-bit
 *   DAC channels (DAC1 = GPIO25, DAC2 = GPIO26) driven by hardware timer
 *   ISRs.  Each ISR reads one sample from a per-channel ring buffer and
 *   writes directly to the DAC register — no mixing in interrupt context.
 *
 *   Timer clock: kClockFreqHz / prescale → kSoundTimerBaseHz (4 MHz on all
 *   current ESP32 variants).  Timer ticks passed to `setRate()` are in
 *   units of this base clock.  The prescaler is computed at init time from
 *   `kClockFreqHz` (board_clock.h) and `kSoundTimerBaseHz` (sound config).
 *
 *   This is a 1:1 iso-functional replacement of the original DiYGuy timer
 *   ISR pair (`variablePlaybackTimer` + `fixedPlaybackTimer`), with the
 *   mixing logic moved to the AudioTask.
 *
 * @code
 *   // In sound hw init (sound_init.cpp — after pin claims):
 *   SoundHalAudio* hal = sound_hal_dac_create(&cfg);
 *   hal->init(hal->ctx);
 *   hal->start(hal->ctx);
 *
 *   // In AudioTask (Core 0, high priority):
 *   hal->write(hal->ctx, kChEngine, engineBuf, n);
 *   hal->setRate(hal->ctx, kChEngine, newTicks);
 * @endcode
 *****************************************************************************/
#pragma once

#include "sound_hal_audio.h"


// =============================================================================
// 1. CONFIGURATION
// =============================================================================

	/// Backend-specific configuration for the ESP32 internal DAC.
struct SoundHalDacCfg {
	uint32_t varInitTicks;    ///< Initial timer period for engine channel (ticks).
	uint32_t fixInitTicks;    ///< Initial timer period for effects channel (ticks).
	uint16_t ringBufSize;     ///< Per-channel ring buffer capacity in samples.
};


// =============================================================================
// 2. FACTORY
// =============================================================================

/**
 * @brief DAC backend factory. Allocate and return a ready-to-init HAL instance.
 *
 * @details The returned pointer is valid for the lifetime of the program
 *   (static allocation, no heap).  Call `hal->init(hal->ctx)` before use.
 *
 * @param cfg  Backend configuration.  Copied internally — caller may discard.
 * @return     Pointer to a fully wired `SoundHalAudio` instance.
 */
SoundHalAudio* sound_hal_dac_create(const SoundHalDacCfg* cfg);

// EOF sound_hal_dac.h
