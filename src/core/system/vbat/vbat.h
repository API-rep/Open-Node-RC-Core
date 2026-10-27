/******************************************************************************
 * @file vbat.h
 * @brief Battery management — top-level module.
 *
 * @details Single entry point for all battery-related functionality.
 *   Composed of two independent sub-modules:
 *
 *   - `vbat_sense` : hardware ADC sensing, sliding average, cell auto-detection,
 *     low-bat flag. Writes `DigitalComBusID::VBAT_FAIL` (see vbat_fail.cb)
 *     as proof-of-life each cycle. Activated by `-D VBAT_LIPO` (or other
 *     VBAT_xxx flags) in platformio.ini. Sets HAS_VBAT_SENSING when active
 *     (defined by the battery profile backend).
 *
 *   - `vbat_alert` : reactions to low battery (beep, sound alert, LED alert, etc.).
 *     Reads `DigitalComBusID::VBAT_FAIL` from the ComBus digital bus and
 *     triggers reactions. Activated by any of the `-D VBAT_ALERT_*` compile flags.
 *
 *   Both sub-modules are optional and can operate independently.
 *   The `VBAT_FAIL` ComBus channel is the pivot of battery sensing:
 *   `vbat_sense` (or any remote node on the bus) writes it, and `vbat_alert`
 *   reads it, regardless of who produced the sensing data.
 *
 *   When neither HAS_VBAT_SENSING nor any VBAT_ALERT_* flag is set, both
 *   functions are inline no-ops.
 *****************************************************************************/
#pragma once

#include <core/system/vbat/vbat_alert.h>
#include <core/system/combus/combus_defs.h>   // ComBus


#if defined(HAS_VBAT_SENSING) || defined(VBAT_ALERT)

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
 * @details Sequences four steps in order:
 *   1. `vbat_sense_tick()` — (if HAS_VBAT_SENSING) ADC read, sliding average,
 *      low-bat detection.
 *   2. (if HAS_VBAT_FAILSAFE) — writes `DigitalComBusID::VBAT_FAIL` from
 *      `vbat_is_low(0)`. This re-arms the VBAT_FAIL contributor each cycle.
 *   3. (future)            — ComBus runlevel sleeping / re-arm.
 *   4. `vbat_alert_tick()`  — read `DigitalComBusID::VBAT_FAIL`, trigger
 *      gated reactions.
 *
 *   In ComBus-only mode, the input bridge must populate
 *   `DigitalComBusID::VBAT_FAIL` before this function is called.
 *
 * @param bus  Node ComBus receiving the `VBAT_FAIL` contributor.
 */

void vbat_update(ComBus& bus);


// =============================================================================
// 2. NO-OP STUBS
// =============================================================================

#else // neither HAS_VBAT_SENSING nor VBAT_ALERT

inline void vbat_init(VBatSense* = nullptr) {}
inline void vbat_update(ComBus&) {}

#endif // HAS_VBAT_SENSING || VBAT_ALERT

// EOF vbat.h
