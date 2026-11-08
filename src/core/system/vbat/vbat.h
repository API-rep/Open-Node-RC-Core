/******************************************************************************
 * @file  vbat.h
 * @brief Battery management: the entry point of the battery modules.
 *
 * @details Two optional sub-modules, sequenced here:
 *   - vbat_sense: the ADC sensing, the sliding average, the cell count detection and
 *     the low battery state.  Active with a battery profile flag in platformio.ini
 *     (-D VBAT_LIPO...), which defines HAS_VBAT_SENSING;
 *   - vbat_alert: the reactions to a low battery (beep, light...).  Active with any
 *     -D VBAT_ALERT_* flag.
 *
 *   With HAS_VBAT_FAILSAFE, vbat_update() also writes VBAT_FAIL, the battery
 *   contributor of the hard failsafe chain: true when the battery is low.
 *
 *   Without HAS_VBAT_SENSING and without any alert, both functions are inline no-ops.
 *****************************************************************************/
#pragma once

#include <core/system/vbat/vbat_alert.h>
#include <core/system/combus/combus_defs.h>   // ComBus


#if defined(HAS_VBAT_SENSING) || defined(VBAT_ALERT)

// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Battery init: the sensing, then the alert hardware.
 *
 * @param sense  Board-defined VBatSense container; nullptr when the board has no
 *               local ADC sensing (alert only).
 */

void vbat_init(VBatSense* sense = nullptr);


/**
 * @brief Battery step, once per main loop: sensing, VBAT_FAIL, alert.
 *
 * @details Call before failsafe_update(): the hard chain reads VBAT_FAIL in the same
 *   cycle.
 *
 * @param bus  ComBus of the node, which receives VBAT_FAIL.
 */

void vbat_update(ComBus& bus);



// =============================================================================
// 2. NO BATTERY MODULE
// =============================================================================

#else // neither HAS_VBAT_SENSING nor VBAT_ALERT

inline void vbat_init(VBatSense* = nullptr) {}
inline void vbat_update(ComBus&) {}

#endif // HAS_VBAT_SENSING || VBAT_ALERT

// EOF vbat.h
