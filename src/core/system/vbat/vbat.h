/******************************************************************************
 * @file vbat.h
 * @brief Battery management — top-level module.
 *
 * @details Single entry point for all battery-related functionality.
 *   Composed of two independent sub-modules:
 *
 *   - `vbat_sense` : hardware ADC sensing, sliding average, cell auto-detection,
 *     low-bat flag. Writes `batteryIsLow` into the ComBus digital bus as event.
 *     Activated by `-D VBAT_SENSING=<TYPE>` compile flag
 *
 *   - `vbat_alert` : reactions to low battery (beep, sound alert, LED alert, etc.).
 *     Reads `batteryIsLow` from the ComBus digital bus and triggers reactions. 
 *     Activated by any of the `-D VBAT_ALERT_*` compile flags.
 *
 *   Both sub-modules are optional and can operate independently.
 *   The ComBus `batteryIsLow` flag is the pivot of battery sensing: `vbat_sense`
 *   (or any remote node on the bus) writes it, and `vbat_alert` reads it,
 *   regardless of who produced the sensing data.
 *
 *   When neither VBAT_SENSING nor any VBAT_ALERT_* flag is set, both
 *   functions are inline no-ops.
 *****************************************************************************/
#pragma once

#include <core/system/vbat/vbat_alert.h>


#if defined(VBAT_SENSING) || defined(VBAT_ALERT)

// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief Battery monitoring init — hardware sensing + alert hardware setup.
 *
 * @details Sequences two optional sub-modules in order:
 *   1. vbat_sense_init() : ADC setup, sliding-average seed, cell auto-detection,
 *      initial low-bat evaluation.
 *   2. vbat_alert_init() : initializes alert hardware (buzzer channel, etc.).
 *
 * @param sense  Board-defined VBatSense container.  Pass `nullptr` when no
 *               local ADC sensing is available (alert-only / ComBus mode).
 */

void vbat_init(VBatSense* sense = nullptr);


/**
 * @brief Main-loop battery tick — single entry point.
 *
 * @details Sequences three steps in order:
 *   1. `vbat_sense_tick()` — (if VBAT_SENSING) ADC read, sliding average,
 *      low-bat detection.
 *   2. (if VBAT_SENSING)    — writes `comBus.batteryIsLow` from local ADC.
 *   3. `vbat_alert_tick()`  — read `comBus.batteryIsLow`, trigger gated reactions.
 *
 *   In ComBus-only mode, the input bridge must populate `comBus.batteryIsLow`
 *   before this function is called.
 */

void vbat_update();


// =============================================================================
// 2. NO-OP STUBS
// =============================================================================

#else // neither VBAT_SENSING nor VBAT_ALERT

inline void vbat_init(VBatSense* = nullptr) {}
inline void vbat_update() {}

#endif // VBAT_SENSING || VBAT_ALERT

// EOF vbat.h
