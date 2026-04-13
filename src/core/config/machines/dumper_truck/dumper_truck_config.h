/*!****************************************************************************
 * @file  dumper_truck_config.h
 * @brief Dumper-truck machine-class sub-umbrella.
 *
 * @details Conditionally includes all config sub-modules for the dumper-truck /
 *          articulated hauler vehicle class.  Consumers include this file via
 *          machine_config.h — they never reference the vehicle-class path
 *          directly.
 *
 *          Sub-modules included:
 *            combus   — always (channel IDs, array externs, comBus extern)
 *            motion   — always (traction preset alias)
 *            inputs_map — if INPUT_MODULE is defined
 *            sound    — if SOUND_NODE or SOUND_ENABLED is defined
 *
 *          NOTE: `sound_dynamics.h` (standalone dispatcher) is no longer
 *          needed — sound profile inclusion is handled here.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. COMBUS  (always)
// =============================================================================

#include <core/config/machines/dumper_truck/combus/dumper_truck.h>


// =============================================================================
// 2. MOTION PRESET ALIAS  (always)
// =============================================================================

#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>


// =============================================================================
// 3. INPUT MAPPING  (only when an input module is active)
// =============================================================================

#ifdef INPUT_MODULE
  #include <core/config/machines/dumper_truck/inputs_map/inputs_map.h>
#endif


// =============================================================================
// 4. SOUND DYNAMICS  (only in sound-module builds)
// =============================================================================

#if defined(SOUND_NODE) || defined(SOUND_ENABLED)
  #include <core/config/machines/dumper_truck/sound/dumper_truck_sound.h>
#endif


// EOF dumper_truck_config.h
