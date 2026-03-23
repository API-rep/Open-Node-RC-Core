/******************************************************************************
 * @file sound_map.h
 * @brief Sound profile dispatcher — selects the active ComBus → sound slot mapping.
 *
 * @details Derives the active sound map from the MACHINE_TYPE build flag,
 *   which is shared between the machine node and the sound node via the
 *   [volvo_A60H_id] identity section in platformio.ini.
 *
 *   MACHINE_TYPE is a text token matching a CombusLayout enum member
 *   (e.g. DUMPER_TRUCK).  Token concatenation resolves it to a preprocessor
 *   integer (no separate flag needed).
 *
 *   Override: -D SOUND_MAP_OVERRIDE='"path/to/custom_map.h"' takes
 *   precedence over the MACHINE_TYPE-derived selection.  Use only when a
 *   vehicle needs a custom mapping that diverges from the default profile.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. MACHINE_TYPE → PREPROCESSOR RESOLVER
// =============================================================================

// Numeric aliases mirroring CombusLayout enum — used for #if dispatch only.
#define _SMAP_UNDEFINED     0
#define _SMAP_DUMPER_TRUCK  1
// Add new machine types here as they are introduced in CombusLayout.

// Resolve MACHINE_TYPE text token (e.g. DUMPER_TRUCK) to its numeric alias
// via token concatenation.  Result: _SMAP_TYPE == _SMAP_DUMPER_TRUCK, etc.
#define _SMAP_CAT_(a, b)    a##b
#define _SMAP_CAT(a, b)     _SMAP_CAT_(a, b)
#define _SMAP_TYPE          _SMAP_CAT(_SMAP_, MACHINE_TYPE)


// =============================================================================
// 2. PROFILE SELECTION
// =============================================================================

#if defined(SOUND_MAP_OVERRIDE)
	// Custom override — add -D SOUND_MAP_OVERRIDE='"path/to/custom_map.h"'
	#include SOUND_MAP_OVERRIDE

#elif _SMAP_TYPE == _SMAP_DUMPER_TRUCK
	#include "dumper_truck/sound_map/sound_map.h"

#else
	#error "No sound map for this MACHINE_TYPE — add a new entry in core/config/combus/sound_map.h"
#endif

// EOF sound_map.h
