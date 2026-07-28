/*!****************************************************************************
 * @file  loader_config.h
 * @brief Wheel loader machine-class sub-umbrella.
 *
 * @details Conditionally includes all config sub-modules for the wheel loader
 *          vehicle class.  Consumers include this file via machine_config.h —
 *          they never reference the vehicle-class path directly.
 *
 *          Sub-modules included:
 *            combus   — TODO winter 2026 (loader channel IDs not yet defined)
 *            motion   — TODO winter 2026
 *            inputs_map — TODO winter 2026
 *            sound    — if SOUND_NODE or SOUND_ENABLED is defined
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. COMBUS  (TODO: winter 2026 — define loader ComBus channel IDs)
// =============================================================================

// #include ...


// =============================================================================
// 2. MOTION PRESET ALIAS  (TODO: winter 2026 — define loader motion channels)
// =============================================================================

// #include ...


// =============================================================================
// 3. INPUT MAPPING  (TODO: winter 2026 — define loader input module mapping)
// =============================================================================

//   #include ...


// =============================================================================
// 4. SOUND DYNAMICS  (only in sound-module builds)
// =============================================================================

#if defined(SOUND_NODE) || defined(SOUND_ENABLED)
  #include <core/config/machines/loader/sound/loader_sound.h>
#endif

// EOF loader_config.h
