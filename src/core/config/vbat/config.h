/******************************************************************************
 * @file config.h
 * @brief Battery sensing — chemistry profile selector.
 *
 * @details Selects the correct battery parameter profile based on the
 *   VBAT_SENSING build flag. Usage in platformio.ini:
 *
 *     -D VBAT_SENSING=LIPO       → LiPo profile (bat_lipo.h)
 *
 *   A bare -D VBAT_SENSING (no type) or an unknown type triggers a
 *   hard build error to prevent silent use of incorrect thresholds.
 *   Battery type tokens are defined in include/defs/core_defs.h.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. SAMPLING PARAMETERS (chemistry-independent)
// =============================================================================

	/// Sliding average buffer depth (samples). Common to all battery chemistries.
inline constexpr uint8_t SamplingDepth = 6;


// =============================================================================
// 2. PROFILE SELECTION
// =============================================================================

#ifdef VBAT_SENSING
	#if VBAT_SENSING == LIPO
		#include "bat_lipo.h"

	//	#elif VBAT_SENSING == LIFE
	//		#include "bat_life.h"

	#else
		#error "Unknown VBAT_SENSING type. Check platformio.ini to fix the problem."
	#endif

#endif // VBAT_SENSING

// EOF config.h
