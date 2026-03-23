/******************************************************************************
 * @file sound_map.h
 * @brief Sound profile dispatcher — selects the active ComBus → sound slot mapping.
 *
 * @details Includes the correct sound_map profile header based on the
 *   SOUND_PROFILE_* build flag.  The flag is declared in the machine identity
 *   section of platformio.ini ([volvo_A60H_id], etc.) so it is shared between
 *   the machine node and its paired sound node without duplication.
 *
 *   Override: -D SOUND_MAP_OVERRIDE='"path/to/custom_map.h"' takes precedence
 *   over the SOUND_PROFILE_* selection.  Use when a vehicle needs a mapping
 *   that diverges from the default profile for its machine type.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. PROFILE SELECTION
// =============================================================================

#if defined(SOUND_MAP_OVERRIDE)
	// Custom override — add -D SOUND_MAP_OVERRIDE='"path/to/custom_map.h"' to the env.
	#include SOUND_MAP_OVERRIDE

#elif defined(SOUND_PROFILE_DUMPER_TRUCK)
	#include "dumper_truck/sound_map/sound_map.h"

#else
	#error "No SOUND_PROFILE defined — add -D SOUND_PROFILE_* to the machine identity section in platformio.ini"
#endif

// EOF sound_map.h
