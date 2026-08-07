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
 *            inputs_map — INSTANCE-specific (see note below)
 *            sound    — if SOUND_NODE or SOUND_ENABLED is defined
 *
 *          NOTE: `sound_dynamics.h` (standalone dispatcher) is no longer
 *          needed — sound profile inclusion is handled here.
 *******************************************************************************
 */
#pragma once

#include <machines/config/machines/machines.h> // MACHINE_TYPE_*


#if defined(MACHINE_TYPE_DUMPER_TRUCK)

  // =============================================================================
  // 1. COMBUS  (always)
  // =============================================================================

  // ---- 1a. Path macros — SINGLE source of truth for the .inc file paths.
  //
  // Every ComBus .inc fragment under dumper_truck/combus/ is referenced by
  // its macro here (and ONLY here).  Consumers (per-instance combus_ids.h,
  // combus.cpp) #include the macros, never a hard-coded path.  This is the
  // canonical point of resolution — adding a new Remote/Local/System .inc
  // requires exactly one new COMBUS_*_INC macro below.
  //
  // Paths are POSIX-style so the C preprocessor can consume them directly
  // in `#include COMBUS_*_INC` directives (incl. inside instance-scoped
  // arrays that live outside dumper_truck/'s include search root).
  //
  //   combus_ids_remote_*.inc  — TYPE-level IDs (one set per enum, shared
  //                                across ALL dumper-truck instances).
  //   combus_remote_*.inc      — TYPE-level non-ID channel fragments.
  //   combus_ids_*_local_*.inc — INSTANCE-level IDs (Volvo A60H Bruder).
  //   combus_channels_*_local_*.inc / combus_channels_*_system_*.inc — not
  //     routed through this file (they live under the per-instance
  //     volvo_A60H_bruder/combus/ folder and are included via the relative
  //     #include "..." form in the instance's combus.cpp / combus_ids.h).

  #define COMBUS_IDS_REMOTE_ANALOG_INC  <core/config/machines/dumper_truck/combus/combus_ids_remote_analog.inc>
  #define COMBUS_IDS_REMOTE_DIGITAL_INC <core/config/machines/dumper_truck/combus/combus_ids_remote_digital.inc>
  #define COMBUS_REMOTE_ANALOG_INC      <core/config/machines/dumper_truck/combus/combus_remote_analog.inc>
  #define COMBUS_REMOTE_DIGITAL_INC     <core/config/machines/dumper_truck/combus/combus_remote_digital.inc>

	// REMOTE-only vocabulary for generic dumper-truck code.  Machine-specific
	// code includes the concrete machine's combus.h directly.
	#include <core/config/machines/dumper_truck/combus/combus_ids_remote.h>

  
  // =============================================================================
  // 2. MOTION PRESET ALIAS  (always)
  // =============================================================================
  
  #include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>
  
  
  // =============================================================================
  // 3. INPUT MAPPING
  // =============================================================================
  // NOTE: inputs_map is now INSTANCE-specific (volvo_A60H_bruder/inputs_map/).
  // The instance's machine header includes it directly.
  
  
  // =============================================================================
  // 4. SOUND DYNAMICS  (only in sound-module builds)
  // =============================================================================
  
  #if defined(SOUND_NODE) || defined(SOUND_ENABLED)
    #include <core/config/machines/dumper_truck/sound/dumper_truck_sound.h>
  #endif
  
#endif  // MACHINE_TYPE_DUMPER_TRUCK

// EOF dumper_truck_config.h