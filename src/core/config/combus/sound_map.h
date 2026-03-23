/******************************************************************************
 * @file sound_map.h
 * @brief Sound profile dispatcher — selects the active ComBus → sound slot mapping.
 *
 * @details Dispatches on MACHINE_TYPE (same constant used by combus_types.h),
 *   so no extra build flag is needed — the machine identity section already
 *   carries MACHINE_TYPE and it is inherited by the paired sound node.
 *
 *   Override: -D SOUND_MAP_OVERRIDE='"path/to/custom_map.h"' takes precedence
 *   over MACHINE_TYPE selection.  Use when a vehicle needs a mapping that
 *   diverges from the default profile for its machine type.
 *
 *   NOTE: DUMPER_TRUCK and sibling constants are defined in
 *         include/defs/core_defs.h, which is reachable before this header
 *         through the combus_types.h → defs.h → core_defs.h include chain.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. PROFILE SELECTION
// =============================================================================

#if defined(SOUND_MAP_OVERRIDE)
	// Custom override — add -D SOUND_MAP_OVERRIDE='"path/to/custom_map.h"' to the env.
	#include SOUND_MAP_OVERRIDE

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
	#include "dumper_truck/sound_map/sound_map.h"

#else
	#error "No sound map matched — check that MACHINE_TYPE is defined and has a dispatch branch in sound_map.h"
#endif

// EOF sound_map.h
