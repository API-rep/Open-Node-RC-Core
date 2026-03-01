/*!****************************************************************************
 * @file  vbat_struct.h
 * @brief Battery voltage sensing structure definitions.
 *
 * @details Pure type definitions — no feature flag dependency.
 *   Available regardless of VBAT_SENSING activation.
 *******************************************************************************///
#pragma once

#include <stdint.h>

#include <core/config/vbat/config.h>


// =============================================================================
// 1. SENSING CONFIGURATION
// =============================================================================

/**
 * @brief Hardware and threshold configuration for one sensing channel.
 *
 * @details Hardware fields (pin, resistors, diode drop, adcRefVoltage) are
 *   defined as named constexpr instances in the board config file (e.g.
 *   VBatMainCfg, VBatSrvACfg in ESP32_8M_6S.h).
 *   Threshold fields (cutoffVolt, chargedVolt, hysteresis, intervalMs) use
 *   the module-level constexpr defaults (VBatCutoffVoltage etc.) — typed,
 *   no macros exposed past vbat_sense.h section 1.
 */

struct VBatSenseConfig {
  const char*    infoName;       ///< Human-readable channel name (e.g. "Main battery").
  const uint8_t  pin;            ///< ADC input GPIO pin.
  const float    adcRefVoltage;  ///< Board logic voltage reference (V) — from AdcRefVoltage in board config.
  const uint32_t hsResOhm;       ///< High-side resistor value (Ω).
  const uint32_t lsResOhm;       ///< Low-side resistor value (Ω).
  const float    diodeDrop;      ///< Forward diode voltage drop (V).
  const float    cutoffVolt;     ///< Low battery threshold per cell (V).
  const float    chargedVolt;    ///< Fully charged voltage per cell (V).
  const float    hysteresis;     ///< Re-arm margin above cutoff (V).
  const uint32_t intervalMs;     ///< Sensing tick period (ms).
};



// =============================================================================
// 2. SENSING STATE
// =============================================================================

/**
 * @brief Runtime state snapshot for one sensing channel.
 *
 * @details Holds all mutable data for sensing process. Written exclusively by 
 *   the vbat module — treat as read-only from callers.
 *   Zeroed by default construction; disabled is set at init if channel is
 *   found unpowered (pull-down → voltage < 0.5 V).
 */
struct VBatSenseState {
  float         voltage;                ///< Last averaged battery voltage (V).
  uint8_t       cells;                  ///< Detected number of series cells (0 = unknown).
  bool          isLow;                  ///< True when voltage is below cutoff.
  bool          disabled;               ///< True when channel was found unpowered at init (auto-detected).
  float         rawVals[SamplingDepth]; ///< Sliding average sample buffer.
  unsigned long lastTickMs;             ///< Timestamp of last tick (ms).
};



// =============================================================================
// 3. TOP CONTAINER
// =============================================================================

/**
 * @brief Top-level battery sensing container.
 *
 * @details cfg points to the board-defined config array (flash).
 *   state points to the module-owned runtime state array (RAM, in vbat_sense.cpp).
 *   count holds the number of active channels.
 */

struct VBatSense {
  const VBatSenseConfig* cfg   = nullptr;  ///< Pointer to board config array (flash).
  uint8_t                count = 0;        ///< Number of active channels.
  VBatSenseState*        state = nullptr;  ///< Pointer to runtime state array (module RAM).
};

// EOF vbat_struct.h
