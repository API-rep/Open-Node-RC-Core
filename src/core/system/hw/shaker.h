/******************************************************************************
 * @file shaker.h
 * @brief Shaker motor — shared configuration type.
 *
 * @details Provides the ShakerCfg struct — per-vehicle shaker intensity levels
 *   (idle, start, full-throttle, stop).  Values are 0–100 % motor power.
 *   One constexpr instance lives in the machine sound config
 *   (`dumper_truck_sound.h` → `shaker_presets.h`).
 *
 *   The HW driver (`shaker_hw_update()`) reads the throttle value directly
 *   from the ComBus analog channel (ENGINE_RPM_BUS) — no intermediate
 *   bus struct is needed while computation and drive both run on the same node.
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. CONFIGURATION
// =============================================================================

/** @brief Per-vehicle shaker motor intensity levels (0–100 %). */
struct ShakerCfg {
	uint8_t idle;         ///< Motor power while idling.
	uint8_t start;        ///< Motor power during engine start.
	uint8_t fullThrottle; ///< Motor power at full throttle.
	uint8_t stop;         ///< Motor power during engine stop.
};

// EOF shaker.h
