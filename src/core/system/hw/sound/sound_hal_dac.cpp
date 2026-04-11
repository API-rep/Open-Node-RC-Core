/******************************************************************************
 * @file sound_hal_dac.cpp
 * Sound module — ESP32 internal DAC audio backend implementation.
 *
 * @details Two minimal timer ISRs consume samples from per-channel ring
 *   buffers and write directly to the DAC registers.  The AudioTask (or
 *   any producer) pushes pre-mixed samples via the `write()` function
 *   pointer; mixing is done task-side — the ISR never does mixing.
 *
 *   SPSC buffer contract (Single Producer Single Consumer):
 *   - Single producer (AudioTask on Core 0) calls `write()`.
 *   - Single consumer (ISR on Core 1) reads in the ISR.
 *   - Lock-free: producer writes `writeCursor`, consumer writes `readCursor`.
 *   - Power-of-two capacity so `& mask` replaces `% size` (no division in ISR).
 *
 *   Static allocation — no heap.  One instance supported (two DAC channels).
 *****************************************************************************/

#include "sound_hal_dac.h"
#include "dac/dac_output.h"                    // dac_output() — register-level DAC write
#include <core/config/hw/board_clock.h>        // kClockFreqHz, kSoundTimerBaseHz
#include <Arduino.h>                           // dacWrite, timerBegin, …


// =============================================================================
// 1. RING BUFFER
// =============================================================================

/**
 * @brief Lock-free SPSC ring buffer for one audio channel.
 *
 * @details Capacity must be a power of two so that index wrap-around uses
 *   a bitwise AND (`index & mask`) instead of a modulo division — this
 *   keeps ISR execution under ~5 instructions with no branch penalty.
 *
 *   The producer (AudioTask) only writes `writeCursor`; the consumer (ISR)
 *   only writes `readCursor`.  Both indices are `volatile` for cross-core
 *   visibility on Xtensa LX6 (dual-core ESP32).
 */
struct RingBuf {
	uint8_t*         buf;           ///< Sample storage (static, not heap).
	uint16_t         mask;          ///< capacity − 1 (power-of-two mask).
	volatile uint16_t writeCursor;  ///< Next write position (producer).
	volatile uint16_t readCursor;   ///< Next read position  (consumer / ISR).
};

	/// Get readable samples from the ring buffer.
static inline uint16_t ring_readable(const RingBuf* ringBuf)
{
	return (ringBuf->writeCursor - ringBuf->readCursor) & ringBuf->mask;
}

	/// Get writable slots from the ring buffer.
static inline uint16_t ring_writable(const RingBuf* ringBuf)
{
	return ringBuf->mask - ((ringBuf->writeCursor - ringBuf->readCursor) & ringBuf->mask);
}



// =============================================================================
// 2. BACKEND STATE
// =============================================================================

	// --- 2.1 Internal limits ---
static constexpr uint16_t kDefaultRingBufSize = 128;    ///< Default ring buffer capacity (power of two).
static constexpr uint16_t kMaxRingBufSize     = 1024;   ///< Ring buffer size upper limit — sizes static storage.
static constexpr uint8_t  kDacSilence         = 128;    ///< DAC midpoint (written when ring is empty).

	// --- 2.2 Runtime state (shared ISR ↔ task) ---
static uint8_t      s_ringBufMem[kChCount][kMaxRingBufSize];  ///< Per-channel sample storage.
static RingBuf      s_ringBuf[kChCount];                      ///< Per-channel ring descriptors.
static hw_timer_t*  s_timer[kChCount];                        ///< Per-channel hardware timer handles.
static uint8_t      s_prescale = 0;                           ///< Timer prescaler (computed at init).
static SoundHalDacCfg s_dacCfg;                               ///< Backend configuration snapshot.

	// --- 2.3 Public instance ---
static SoundHalAudio s_soundHal;  ///< HAL vtable (returned by factory).



// =============================================================================
// 3. ISR — ENGINE CHANNEL (DAC1 / VARIABLE RATE)
// =============================================================================

/**
 * @brief Engine channel ISR. Read one sample from ring, write to DAC1.
 *
 * @details ~5 instructions.  If the ring is empty the ISR outputs silence
 *   (DAC midpoint 128) — no audible pop, just a brief gap.
 */

static void IRAM_ATTR isr_engine()
{
	RingBuf* ringBuf = &s_ringBuf[kChEngine];
	uint8_t sample = kDacSilence;

		// Consume one buffersample if available, otherwise keep silence.
	if (ringBuf->readCursor != ringBuf->writeCursor) {
		sample = ringBuf->buf[ringBuf->readCursor & ringBuf->mask];
		ringBuf->readCursor = (ringBuf->readCursor + 1) & ringBuf->mask;
	}
	dac_output(kChEngine, sample);
}


// =============================================================================
// 4. ISR — EFFECTS CHANNEL (DAC2 / FIXED RATE)
// =============================================================================

/**
 * @brief Effects channel ISR. Read one sample from ring, write to DAC2.
 *
 * @details Same structure as isr_engine(), targeting DAC2.
 */

static void IRAM_ATTR isr_effects()
{
	RingBuf* ringBuf = &s_ringBuf[kChEffects];
	uint8_t sample = kDacSilence;

		// Consume one buffer sample if available, otherwise keep silence.
	if (ringBuf->readCursor != ringBuf->writeCursor) {
		sample = ringBuf->buf[ringBuf->readCursor & ringBuf->mask];
		ringBuf->readCursor = (ringBuf->readCursor + 1) & ringBuf->mask;
	}
	dac_output(kChEffects, sample);
}


// =============================================================================
// 5. HAL FUNCTION IMPLEMENTATIONS
// =============================================================================

/**
 * @brief Set up DAC hardware, timers, and ring buffers.
 *
 * @details Steps:
 *   1. Compute prescaler from board clock and target base.
 *   2. Init ring buffers (zero-fill, reset indices).
 *   3. Enable DAC channels via Arduino API (configures internal routing).
 *   4. Create hardware timers with computed prescaler.
 *   5. Attach ISRs and set initial alarm periods.
 * 
 *   Timers are NOT started here — call start() separately.
 */
static void dac_init(void* /*ctx*/)
{
	  // --- 1. Compute prescaler ---
	s_prescale = static_cast<uint8_t>(kClockFreqHz / kSoundTimerBaseHz);

	  // --- 2. Init ring buffers ---
	uint16_t cap = s_dacCfg.ringBufSize;

	if (cap == 0 || cap > kMaxRingBufSize)
		cap = kDefaultRingBufSize;

	  // Round up bufCap to next power of two, or clamp to max if too large.
	uint16_t bufCap = 1;
	while (bufCap < cap) bufCap <<= 1;
	if (bufCap > kMaxRingBufSize) bufCap = kMaxRingBufSize;

	for (uint8_t ch = 0; ch < kChCount; ++ch) {
		s_ringBuf[ch].buf  = s_ringBufMem[ch];
		s_ringBuf[ch].mask = bufCap - 1;
		s_ringBuf[ch].writeCursor = 0;
		s_ringBuf[ch].readCursor = 0;
    
		for (uint16_t i = 0; i < bufCap; ++i)
			s_ringBufMem[ch][i] = kDacSilence;
	}

	  // --- 3. Enable DAC channels (Arduino internal routing) ---
	dacWrite(DAC1, 0);
	dacWrite(DAC2, 0);

	  // --- 4. Create timers ---
	s_timer[kChEngine]  = timerBegin(0, s_prescale, true);
	s_timer[kChEffects] = timerBegin(1, s_prescale, true);

	  // --- 5. Attach ISRs and set initial periods ---
	timerAttachInterrupt(s_timer[kChEngine],  &isr_engine,  true);
	timerAlarmWrite(s_timer[kChEngine],  s_dacCfg.varInitTicks, true);

	timerAttachInterrupt(s_timer[kChEffects], &isr_effects, true);
	timerAlarmWrite(s_timer[kChEffects], s_dacCfg.fixInitTicks, true);
}



/**
 * @brief Enable audio output — start both timer ISRs.
 */

static void dac_start(void* /*ctx*/)
{
	timerAlarmEnable(s_timer[kChEngine]);
	timerAlarmEnable(s_timer[kChEffects]);
}



/**
 * @brief Disable audio output — stop timers and silence DACs.
 */

static void dac_stop(void* /*ctx*/)
{
	timerAlarmDisable(s_timer[kChEngine]);
	timerAlarmDisable(s_timer[kChEffects]);

	dac_output(kChEngine,  kDacSilence);
	dac_output(kChEffects, kDacSilence);
}



/**
 * @brief Change timer period for the given channel.
 *
 * @details Called from the AudioTask (or loop) to adjust engine RPM playback
 *   speed.  Writes the new alarm period — the timer auto-reloads on next tick.
 */

static void dac_setRate(void* /*ctx*/, SoundHalCh ch, uint32_t timerTicks)
{
	if (ch >= kChCount) return;
	timerAlarmWrite(s_timer[ch], timerTicks, true);
}



/**
 * @brief Return how many samples can be pushed before the ring is full.
 */

static uint16_t dac_writable(void* /*ctx*/, SoundHalCh ch)
{
	if (ch >= kChCount) return 0;
	return ring_writable(&s_ringBuf[ch]);
}



/**
 * @brief Push samples into the ring buffer for the given channel.
 *
 * @details Copies up to @p count samples.  Returns the number actually
 *   written (may be less if ring is nearly full).  Non-blocking — the
 *   producer should call `writable()` first to avoid partial writes.
 */

static uint16_t dac_write(void* /*ctx*/, SoundHalCh ch, const uint8_t* samples, uint16_t count)
{
	if (ch >= kChCount || !samples) return 0;

	RingBuf* ringBuf = &s_ringBuf[ch];
	uint16_t avail = ring_writable(ringBuf);
	if (count > avail) count = avail;

	for (uint16_t i = 0; i < count; ++i) {
		ringBuf->buf[ringBuf->writeCursor & ringBuf->mask] = samples[i];
		ringBuf->writeCursor = (ringBuf->writeCursor + 1) & ringBuf->mask;
	}
	return count;
}


// =============================================================================
// 6. PUBLIC API
// =============================================================================

/**
 * @brief Create the DAC backend and return a ready-to-init HAL instance.
 *
 * @details Copies cfg internally, wires all function pointers into the
 *   static SoundHalAudio instance.  Call hal->init(hal->ctx) after this.
 */
SoundHalAudio* sound_hal_dac_create(const SoundHalDacCfg* cfg)
{
	if (cfg)
		s_dacCfg = *cfg;

	s_soundHal.ctx      = nullptr;
	s_soundHal.init     = dac_init;
	s_soundHal.start    = dac_start;
	s_soundHal.stop     = dac_stop;
	s_soundHal.setRate  = dac_setRate;
	s_soundHal.writable = dac_writable;
	s_soundHal.write    = dac_write;

	return &s_soundHal;
}

// EOF sound_hal_dac.cpp
