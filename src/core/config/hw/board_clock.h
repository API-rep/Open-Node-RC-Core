/******************************************************************************
 * @file board_clock.h
 * @brief Peripheral clock frequency resolver for timer-based modules.
 *
 * @details Provides `kClockFreqHz` — the peripheral bus clock frequency used
 *   by all hardware timer configurations (sound DAC, PWM, etc.).
 *
 *   Resolution order (first match wins):
 *     1. `APB_CLK_FREQ` — SDK define (ESP-IDF `<soc/soc.h>`), source of truth.
 *     2. `BOARD_CLOCK_FREQ_HZ` — board header fallback for non-ESP-IDF targets.
 *     3. Compile error if neither is available.
 *
 *   Board headers may optionally define `BOARD_CLOCK_FREQ_HZ` for portability
 *   to platforms without an ESP-IDF SDK.  On standard ESP32 builds this is
 *   unnecessary — `APB_CLK_FREQ` is always available.
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. SDK PROBE
// =============================================================================

#if __has_include(<soc/soc.h>)
#include <soc/soc.h>
#endif


// =============================================================================
// 2. CLOCK FREQUENCY RESOLUTION
// =============================================================================

#if defined(APB_CLK_FREQ)
	/// Peripheral clock frequency (Hz) — resolved from SDK.
static constexpr uint32_t kClockFreqHz = APB_CLK_FREQ;

#elif defined(BOARD_CLOCK_FREQ_HZ)
	/// Peripheral clock frequency (Hz) — resolved from board header fallback.
static constexpr uint32_t kClockFreqHz = BOARD_CLOCK_FREQ_HZ;

#else
#error "No clock source: define APB_CLK_FREQ (SDK) or BOARD_CLOCK_FREQ_HZ (board header)"
#endif


// =============================================================================
// 3. AUDIO TIMER BASE CLOCK
// =============================================================================

	/// Target base clock for audio hardware timers (Hz).
	/// Prescaler derived at init: kClockFreqHz / kSoundTimerBaseHz.
static constexpr uint32_t kSoundTimerBaseHz = 4000000;

// EOF board_clock.h
