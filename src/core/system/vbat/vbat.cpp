/******************************************************************************
 * @file vbat.cpp
 * @brief Battery management — top-level implementation.
 *
 * @details Sequences `vbat_sense` (hardware ADC init + runtime tick) and
 *   `vbat_alert` (alert hardware init + runtime reactions), each gated by
 *   their respective compile flags.
 *
 *   When only VBAT_ALERT_* flags are set (no VBAT_SENSING), the sensing
 *   steps are skipped and `sense` may be nullptr.  `comBus.batteryIsLow`
 *   is then populated externally (ComBus RX) before each `vbat_update()`.
 *****************************************************************************/

#include "vbat.h"

#ifdef VBAT_SENSING
#include <core/config/combus/combus_types.h>   // comBus.batteryIsLow
#endif

#if defined(VBAT_SENSING) || defined(VBAT_ALERT)

// =============================================================================
// 1. INITIALIZATION
// =============================================================================

/**
 * @brief Battery monitoring init — hardware sensing + alert hardware setup.
 *
 * @details Sequences:
 *   1. `vbat_sense_init()` — (if @p sense is non-null and VBAT_SENSING is
 *      set) ADC setup, sliding-average seed, cell detection.
 *   2. `vbat_alert_init()` — initializes alert hardware (buzzer channel, etc.).
 *
 * @param sense  Board-defined VBatSense container. nullptr disables battery sensing.
 */

void vbat_init(VBatSense* sense)
{
		// --- 1. Hardware sensing (if available) ---
#ifdef VBAT_SENSING
	if (sense) {
		vbat_sense_init(*sense);
	}
#endif

		// --- 2. Alert hardware init ---
	vbat_alert_init();
}


// =============================================================================
// 2. RUNTIME UPDATE
// =============================================================================

/**
 * @brief Main-loop battery tick — single entry point.
 *
 * @details Sequences:
 *   1. `vbat_sense_tick()`  — (if VBAT_SENSING) ADC read, sliding average,
 *      low-bat detection.
 *   2.  (future)            — ComBus runlevel sleeping / re-arm.
 *   3. `vbat_alert_tick()`  — read comBus.batteryIsLow + trigger gated reactions.
 */

void vbat_update()
{
		// --- 1. Sensing (if available) ---
#ifdef VBAT_SENSING
	vbat_sense_tick();
	comBus.batteryIsLow = vbat_is_low(0);
#endif

		// --- 2. (future: ComBus runlevel) ---

		// --- 3. Alert reactions ---
	vbat_alert_tick();
}

#endif // VBAT_SENSING || VBAT_ALERT

// EOF vbat.cpp
