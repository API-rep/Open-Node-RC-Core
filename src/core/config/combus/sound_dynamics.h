/*!****************************************************************************
 * @file    sound_dynamics.h
 * @brief   ComBus config — engine dynamics profile dispatcher.
 *
 * @details Selects the active sound dynamics profile based on MACHINE_TYPE.
 *          Each profile exposes `kVehicleSoundDynamics` — a const struct
 *          containing engine mass tuning (acc / dec), clutch engaging point,
 *          max-RPM percentage, and SEMI_AUTO shift speed thresholds.
 *
 *          This file belongs to the core/combus config layer.
 *          It is temporarily consumed by the sound_module environment via
 *          the `-I src` include flag already present in sound_node_base.
 *          Once the sound module is merged into the machine environment this
 *          file will be reached naturally without any extra include path.
 *
 *          Usage:
 *          @code
 *          #include <core/config/combus/sound_dynamics.h>
 *          // then use kVehicleSoundDynamics.engineAcc, .upShift[], etc.
 *          @endcode
 *
 *          Override: -D SOUND_DYNAMICS_OVERRIDE='"path/to/custom.h"' takes
 *          precedence over MACHINE_TYPE selection.
 *
 *          NOTE: DUMPER_TRUCK and sibling constants are defined in
 *                include/defs/core_defs.h, reachable here via the same
 *                combus_types.h → defs.h → core_defs.h include chain as
 *                profiles.h (resolved before this file when included from
 *                config.h or after a prior vehicle-profile include).
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. PROFILE SELECTION
// =============================================================================

#if defined(SOUND_DYNAMICS_OVERRIDE)
	// Custom override — add -D SOUND_DYNAMICS_OVERRIDE='"path/to/custom.h"' to the env.
	#include SOUND_DYNAMICS_OVERRIDE

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
	#include <core/config/combus/dumper_truck/sound/dumper_truck_sound.h>

#else
	#error "No sound dynamics profile matched — check that MACHINE_TYPE is defined and has a dispatch branch in sound_dynamics.h"
#endif

// EOF sound_dynamics.h
