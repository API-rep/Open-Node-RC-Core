/*!****************************************************************************
 * @file    sound_map.h
 * @brief   Dumper truck — ComBus → rc_engine_sound slot mapping.
 *
 * @details Describes both sides of the sound HAL channel mapping for the
 *          Dumper Truck machine type:
 *
 *          ComBus source (SOUND_CB_*)
 *            Which AnalogComBusID / DigitalComBusID channel feeds which
 *            sound role.  Changes when the ComBus layout is revised.
 *
 *          rc_engine_sound destination (SOUND_CH_*)
 *            Which pulseWidth[] slot each role maps to.  Tied to the
 *            FLYSKY_FS_I6X remote profile declared in 2_Remote.h.
 *            If the profile changes, update these indices accordingly and
 *            rely on the static_assert guards in sound_init.h to catch
 *            any mismatch at compile time.
 *
 *          Future fine-tuning (sound profiles, volumes, RPM curves):
 *            → sound_tune.h  (next to this file, not yet implemented)
 *
 * @note    sound_init.h adds static_asserts cross-checking SOUND_CH_*
 *          against the 2_Remote.h defines (STEERING, THROTTLE, GEARBOX…).
 *          Those asserts cannot live here because 2_Remote.h is only
 *          available in the sound_module translation unit.
 *******************************************************************************
 */
#pragma once

#include <stdint.h>
#include <core/config/combus/combus_types.h>    // AnalogComBusID, DigitalComBusID


// =============================================================================
// 1. COMBUS SOURCE  (which channel feeds which sound role)
// =============================================================================

/// Drive speed → engine throttle sound.
#define SOUND_CB_THROTTLE       AnalogComBusID::DRIVE_SPEED_BUS

/// Steering angle → steering sound (optional, vehicle dependent).
#define SOUND_CB_STEERING       AnalogComBusID::STEERING_BUS

/// Dump body position → hydraulic actuator sound.
#define SOUND_CB_HYDRAULIC      AnalogComBusID::DUMP_BUS

/// Horn trigger.
#define SOUND_CB_HORN           DigitalComBusID::HORN

/// Lights on/off → auxiliary sound (jake brake, beacon).
#define SOUND_CB_LIGHTS         DigitalComBusID::LIGHTS

/// Ignition key → engine start / stop.
#define SOUND_CB_KEY            DigitalComBusID::KEY

/// Left indicator.
#define SOUND_CB_INDICATOR_L    DigitalComBusID::INDICATOR_LEFT

/// Right indicator.
#define SOUND_CB_INDICATOR_R    DigitalComBusID::INDICATOR_RIGHT

/// Hazard lights (both indicators simultaneously).
#define SOUND_CB_HAZARDS        DigitalComBusID::HAZARDS


// =============================================================================
// 2. RC_ENGINE_SOUND DESTINATION  (pulseWidth[] slot indices)
//
//    Profile: FLYSKY_FS_I6X  (2_Remote.h defines STEERING=1, THROTTLE=3, …)
//    Cross-checked by static_asserts in sound_init.h.
// =============================================================================

#define SOUND_CH_STEERING       1u   ///< pulseWidth[1] — steering
#define SOUND_CH_FUNCTION_R     2u   ///< pulseWidth[2] — FUNCTION_R (lights / jake brake)
#define SOUND_CH_THROTTLE       3u   ///< pulseWidth[3] — throttle
#define SOUND_CH_FUNCTION_L     4u   ///< pulseWidth[4] — FUNCTION_L (indicators, encoded)
#define SOUND_CH_HORN           5u   ///< pulseWidth[5] — horn
#define SOUND_CH_HYDRAULIC      6u   ///< pulseWidth[6] — GEARBOX slot reused for hydraulic

// EOF sound_map.h
