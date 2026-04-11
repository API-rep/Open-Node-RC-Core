/******************************************************************************
 * @file sound_hal_audio.h
 * Sound module — audio output hardware abstraction layer.
 *
 * @details Generic function-pointer interface (same pattern as NodeCom) that
 *   decouples the AudioTask (mixing logic) from the physical output backend.
 *
 *   Implementations:
 *   - `SoundHalDac` — ESP32 internal DAC (8-bit, two timer ISRs).
 *   - `SoundHalI2s` — external DAC via I2S DMA (future).
 *
 *   The AudioTask fills a ring buffer via `write()`; the ISR (DAC) or DMA
 *   (I2S) consumes it.  The HAL owns the ring buffers internally — callers
 *   never touch ISR-side pointers directly.
 *
 *   Channel model:
 *   - `kChEngine`  — variable-rate engine sound (DAC1 / I2S left).
 *   - `kChEffects` — fixed-rate auxiliary sounds (DAC2 / I2S right).
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. CHANNEL IDENTIFIERS
// =============================================================================

	/// Audio output channel selector.
enum SoundHalCh : uint8_t {
	kChEngine  = 0,   ///< Variable-rate engine channel (DAC1 / I2S left).
	kChEffects = 1,   ///< Fixed-rate effects channel   (DAC2 / I2S right).
	kChCount   = 2
};


// =============================================================================
// 2. AUDIO HAL INTERFACE
// =============================================================================

/**
 * @brief Audio output HAL. Function-pointer table filled by each backend.
 *
 * @details Returned by the init/runtime function (`sound_hal_dac_create`, …).
 *   The AudioTask retains the pointer for the lifetime of the program.
 *   All function pointers receive `ctx` as first argument (opaque backend
 *   state, same convention as `NodeCom`).
 *
 * @var SoundHalAudio::ctx        Opaque pointer passed to every function below.
 * @var SoundHalAudio::init       Set up hardware (DAC/I2S, timers, DMA). Call once.
 * @var SoundHalAudio::start      Enable audio output (ISR / DMA start).
 * @var SoundHalAudio::stop       Disable output and silence both channels.
 * @var SoundHalAudio::setRate    Change output sample rate for @p ch (timer ticks at
 *                                the configured clock base). No-op on I2S backends.
 * @var SoundHalAudio::writable   Samples that can be pushed to @p ch without blocking.
 * @var SoundHalAudio::write      Push up to @p count samples into @p ch ring buffer.
 *                                Returns the number of samples actually written.
 */

struct SoundHalAudio {
	void*      ctx;
	void     (*init)     (void* ctx);
	void     (*start)    (void* ctx);
	void     (*stop)     (void* ctx);
	void     (*setRate)  (void* ctx, SoundHalCh ch, uint32_t timerTicks);
	uint16_t (*writable) (void* ctx, SoundHalCh ch);
	uint16_t (*write)    (void* ctx, SoundHalCh ch, const uint8_t* samples, uint16_t count);
};

// EOF sound_hal_audio.h
