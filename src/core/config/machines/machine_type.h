/**
 * @file  machine_type.h
 * @brief Top-level machine-class dispatcher — routes MACHINE_TYPE_* to the
 *        matching <machine_type>_config.h sub-umbrella.
 *
 * @details Mirror of the `MACHINE_* -> MACHINE_TYPE_*` two-stage pattern
 *   already in place at the vehicle level (see
 *   src/machines/config/machines/machines.h):
 *
 *     -D MACHINE_VOLVO_A60_H_BRUDER    (CPP -D from platformio.ini [env])
 *       │
 *       └─►  #define MACHINE_TYPE_DUMPER_TRUCK   (volvo_A60H_bruder.h)
 *             │
 *             └─►  THIS FILE dispatches by MACHINE_TYPE_*
 *                   and pulls in dumper_truck/dumper_truck_config.h
 *
 *   Consumers should include ONLY this header — never the per-machine-type
 *   path directly.  This keeps every MACHINE_TYPE_* dispatch logic
 *   confined to a single umbrella.
 *
 *   Dispatch chain (machine-class → TYPE → per-TYPE config):
 *
 *     machine_type.h             ← (this file, top-level)
 *       └── dumper_truck/dumper_truck_config.h
 *             ├── dumper_truck/combus/combus_ids_remote.h    ← REMOTE vocab (IDs)
 *             ├── dumper_truck/motion/dumper_truck_motion.h
 *             └── dumper_truck/sound/dumper_truck_sound.h    ← if SOUND_NODE / SOUND_ENABLED
 *
 *   REMOTE-only ComBus runtime is intentionally NOT instantiated at TYPE
 *   level — each environment (machine, future remote) owns its own combus
 *   arrays and comBus instance via its instance-specific combus.cpp.
 *   TYPE-level only exposes the shared channel vocabulary
 *   (combus_ids_remote.h).  The root combus_ids_remote.h dispatcher is
 *   gone (see commit `13b4c5f` for the cleanup history).

 *
 *   @warning Do NOT add per-machine-type includes here.  Each
 *     <machine_type>_config.h pulls its own sub-modules.
 */

#pragma once

#include <machines/config/machines/machines.h> // MACHINE_TYPE_*


// =============================================================================
// Dispatch by MACHINE_TYPE_* — single ladder for machine-class includes.
// =============================================================================

#if defined(MACHINE_TYPE_DUMPER_TRUCK)
  #include <core/config/machines/dumper_truck/dumper_truck_config.h>

// #elif defined(MACHINE_TYPE_EXCAVATOR)
//   #include <core/config/machines/excavator/excavator_config.h>

// #elif defined(MACHINE_TYPE_WHEEL_LOADER)
//   #include <core/config/machines/loader/loader_config.h>

#else
  #error "Unsupported/missing MACHINE_TYPE_* value. Check MACHINE_* config file to fix the problem"

#endif  // MACHINE_TYPE_*

// EOF machine_type.h