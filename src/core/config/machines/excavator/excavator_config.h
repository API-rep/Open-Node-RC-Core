/*!****************************************************************************
 * @file  excavator_config.h
 * @brief Excavator machine-class sub-umbrella.
 *
 * @details Conditionally includes all config sub-modules for the hydraulic-arm
 *          excavator vehicle class.  Consumers include this file via
 *          machine_config.h — they never reference the vehicle-class path
 *          directly.
 *
 *          Sub-modules included:
 *            combus   — TODO winter 2026 (excavator channel IDs not yet defined)
 *            motion   — TODO winter 2026
 *            inputs_map — TODO winter 2026
 *            sound    — if SOUND_NODE or SOUND_ENABLED is defined
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. COMBUS  (TODO: winter 2026 — define excavator ComBus channel IDs)
// =============================================================================

// #include <core/config/machines/excavator/combus/excavator.h>


// =============================================================================
// 2. MOTION PRESET ALIAS  (TODO: winter 2026)
// =============================================================================

// #include <core/config/machines/excavator/motion/excavator_motion.h>


// =============================================================================
// 3. INPUT MAPPING  (TODO: winter 2026)
// =============================================================================

// #ifdef INPUT_MODULE
//   #include <core/config/machines/excavator/inputs_map/inputs_map.h>
// #endif


// =============================================================================
// 4. SOUND DYNAMICS  (only in sound-module builds)
// =============================================================================

#if defined(SOUND_NODE) || defined(SOUND_ENABLED)
  #include <core/config/machines/excavator/sound/excavator_sound.h>
#endif

// EOF excavator_config.h
