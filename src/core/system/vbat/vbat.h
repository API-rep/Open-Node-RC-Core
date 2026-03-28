/******************************************************************************
 * @file vbat.h
 * @brief Battery management — top-level module.
 *
 * @details Single entry point for all battery-related functionality.
 *   Composed of two independent, optional sub-modules:
 *
 *   - `vbat_sense` (activated by `-D VBAT_SENSING=<TYPE>`)
 *     ADC read, sliding average, cell detection, low-bat flag.
 *     Writes `batteryIsLow` into the ComBus digital bus as event.
 *
 *   - `vbat_alert` (each reaction gated by its own compile flag:
 *     `VBAT_ALERT_BEEP`, `VBAT_ALERT_SOUND`, `VBAT_ALERT_LIGHT`)
 *     Reads `comBus.batteryIsLow` and triggers reactions.
 *
 *   Both sub-modules are optional and can operate independently.
 *   The ComBus `batteryIsLow` flag is the pivot: `vbat_sense` (or any
 *   remote node on the bus) writes it, and `vbat_alert` reads it —
 *   regardless of who produced the sensing data.  A sound module
 *   running standalone senses and reacts locally; one receiving ComBus
 *   frames from a machine reacts to the remote flag without sensing.
 *
 *   ComBus runlevel reaction (sleeping / re-arm) to `batteryIsLow`
 *   is machine-config dependent and will be gated by a dedicated
 *   compile flag (future `-D VBAT_COMBUS_RUNLEVEL`).
 *   That logic lives here at the `vbat` root level, between the
 *   sense and alert steps of `vbat_update()`.
 *
 *   API:
 *     `vbat_init()`   — hardware ADC setup (sense) + app-level boot (alert).
 *     `vbat_update()`  — main-loop tick: sense → (runlevel) → alert.
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

#else // neither VBAT_SENSING nor VBAT_ALERT

// =============================================================================
// 2. NO-OP STUBS
// =============================================================================

inline void vbat_init(VBatSense* = nullptr) {}
inline void vbat_update() {}

#endif // VBAT_SENSING || VBAT_ALERT

// EOF vbat.h
