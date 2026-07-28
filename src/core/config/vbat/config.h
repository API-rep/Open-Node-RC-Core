/******************************************************************************
 * @file config.h
 * @brief Battery sensing — chemistry profile selector.
 *
 * @details Selects the correct battery parameter profile based on the
 *   VBAT_LIPO build flag. Usage in platformio.ini:
 *
 *     -D VBAT_LIPO       → LiPo profile (bat_lipo.h)
 *
 *   A bare -D VBAT_LIPO triggers the LiPo profile inclusion.
 *   Battery type tokens are defined in include/defs/core_defs.h.
 *
 *   New battery profiles can be added by:
 *     1. Creating a new flag (e.g., -D VBAT_LIFE) in platformio.ini
 *     2. Adding a corresponding #define HAS_VBAT_SENSING in the backend header
 *     3. Uncommenting/adding a branch in the dispatch below
 *     4. Including the new profile header
 *
 *   Architecture:
 *     - HAS_VBAT_SENSING (backend flag) is defined by each battery profile header
 *     - Build flags in platformio.ini use the simple -D VBAT_LIPO or similar pattern
 *****************************************************************************/
#pragma once

#include <cstdint>

// =============================================================================
// 1. SAMPLING PARAMETERS (chemistry-independent)
// =============================================================================

/// Sliding average buffer depth (samples). Common to all battery chemistries.
inline constexpr uint8_t SamplingDepth = 6;


// =============================================================================
// 2. PROFILE SELECTION
// =============================================================================

#if defined(VBAT_LIPO)
  #include "bat_lipo.h"

// #elif defined(VBAT_LIFE)
//   #include "bat_life.h"

 #elif defined(VBAT_NONE)
   // No backend; VBAT sensing disabled by missing HAS_VBAT_SENSING flag.

#else
  #error "src/core/config/vbat/config.h: no VBAT_xxx flag defined — set one in platformio.ini (e.g., -D VBAT_LIPO)."
#endif

// EOF config.h
