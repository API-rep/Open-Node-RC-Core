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

// EOF bat_lipo.h
