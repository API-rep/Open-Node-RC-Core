/******************************************************************************
 * @file bat_lipo.h
 * @brief Battery sensing — LiPo cell parameters.
 *
 * @details Fixed constants for Lithium Polymer chemistry.
 *   Values are set conservatively; do not relax them without checking
 *   the cell datasheet and the physical voltage divider on the board.
 *   To use this profile: -D VBAT_SENSING=LIPO in platformio.ini.
 *****************************************************************************/
#pragma once

#include <stdint.h>


// =============================================================================
// 1. LIPO SENSING PARAMETERS
// =============================================================================

	/// Sensing tick period (ms).
inline constexpr uint32_t VBatSenseInterval  = 100;

	/// Low-battery threshold per cell (V). Never set below 3.2 V for LiPo.
inline constexpr float    VBatCutoffVoltage  = 3.3f;

	/// Fully charged voltage per cell (V). Never set above 4.2 V for LiPo.
inline constexpr float    VBatChargedVoltage = 4.2f;

	/// Re-arm margin above cutoff (V). Prevents rapid toggling at threshold.
inline constexpr float    VBatHysteresis     = 0.2f;

	/// Human-readable chemistry type name for display (used by dashboard).
inline constexpr const char* VBatTechName = "LiPo";


// =============================================================================
// 2. LIPO SAFETY CHECKS
// =============================================================================

  // LiPo hard physical limits (Espressif / cell datasheet):
  //   Cutoff  : never below 2.5 V/cell (irreversible damage below ~2.7 V)
  //   Charged : never above 4.25 V/cell (thermal runaway risk above 4.2 V)
  //   Hysteresis must be positive and narrower than the usable voltage window.
static_assert(VBatCutoffVoltage  >= 2.5f,
              "VBatCutoffVoltage below 2.5 V/cell — risk of irreversible LiPo cell damage");
static_assert(VBatChargedVoltage <= 4.25f,
              "VBatChargedVoltage above 4.25 V/cell — thermal runaway risk");
static_assert(VBatCutoffVoltage  <  VBatChargedVoltage,
              "VBatCutoffVoltage must be strictly less than VBatChargedVoltage");
static_assert(VBatHysteresis     >  0.0f,
              "VBatHysteresis must be positive");
static_assert(VBatHysteresis     <  (VBatChargedVoltage - VBatCutoffVoltage),
              "VBatHysteresis is wider than the usable voltage window");
static_assert(VBatSenseInterval  >  0u,
              "VBatSenseInterval must be non-zero");

// EOF bat_lipo.h
