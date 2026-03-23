/******************************************************************************
 * @file sound_map.h
 * @brief Sound profile dispatcher — selects the active ComBus → sound slot mapping.
 *
 * @details Includes the correct sound_map profile header based on the
 *   SOUND_PROFILE_* build flag set in platformio.ini.
 *   The selected header exposes SOUND_CB_* (ComBus source channels) and
 *   SOUND_CH_* (rc_engine_sound pulseWidth[] slot indices).
 *
 *   Define exactly one profile flag in the active environment:
 *     -D SOUND_PROFILE_DUMPER_TRUCK
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. PROFILE SELECTION
// =============================================================================

#if defined(SOUND_PROFILE_DUMPER_TRUCK)
	#include "dumper_truck/sound_map/sound_map.h"
#else
	#error "No SOUND_PROFILE defined — add -D SOUND_PROFILE_DUMPER_TRUCK (or another profile) to your environment in platformio.ini"
#endif

// EOF sound_map.h
