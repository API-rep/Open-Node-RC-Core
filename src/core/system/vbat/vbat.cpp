/******************************************************************************
 * @file vbat.cpp
 * @brief Battery management — top-level implementation.
 *
 * @details Sequences `vbat_sense` (hardware ADC init + runtime tick) and
 *   `vbat_alert` (alert hardware init + runtime reactions), each gated by
 *   their respective compile flags.
 *
 *   When only VBAT_ALERT_* flags are set (no HAS_VBAT_SENSING), the sensing
 *   steps are skipped and `sense` may be nullptr.  `FAILSAFE_VBAT`
 *   is then populated externally (ComBus RX) before each `vbat_update()`.
 *
 *   Since the BATTERY_LOW channel and comBus.batteryIsLow field have been
 *   removed (A16.1), the vbat module now writes FAILSAFE_VBAT directly:
 *     - low  (=false) when the cell voltage drops below threshold
 *     - high (=true)  when the cell is healthy
 *   The FAILSAFE aggregator consumes FAILSAFE_VBAT each cycle.
 *****************************************************************************/

#include "vbat.h"

// Include config.h first so HAS_VBAT_SENSING is defined before we test it
#include <core/config/vbat/config.h>

#if defined(HAS_VBAT_SENSING) || defined(HAS_VBAT_FAILSAFE)
#include <core/config/machines/combus_types.h>     // DigitalComBusID::FAILSAFE_VBAT (machine family dispatch)
#include <core/system/combus/combus_access.h>      // combus_set_digital()
extern ComBus comBus;
#endif

#if defined(HAS_VBAT_SENSING) || defined(VBAT_ALERT)

// =============================================================================
// 1. INITIALIZATION
// =============================================================================

/**
 * @brief Battery monitoring init — hardware sensing + alert hardware setup.
 *
 * @details Initialization sequences two optional sub-modules in order:
 *   1. `vbat_sense_init()` : ADC setup, sliding-average seed, cell auto-detection,
 *      low-bat evaluation.  Activated when @p sense is non-null and HAS_VBAT_SENSING is set.
 *   2. `vbat_alert_init()` : initializes alert hardware (buzzer channel, etc.).
 *      Activated when any VBAT_ALERT_* flag is set.
 *
 * @param sense  Board-defined VBatSense container. nullptr disables battery sensing.
 */

void vbat_init(VBatSense* sense)
{
		// --- 1. Hardware sensing (if available) ---
#ifdef HAS_VBAT_SENSING
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
 * @brief Main-loop battery monitoring routine.
 *
 * @details Single entry point for runtime battery monitoring. Sequences:
 *   1. vbat_sense_tick() : battery sensing
 *   2.  (future)            — ComBus runlevel sleeping / re-arm.
 *   3. vbat_alert_tick() : alert reactions
 */

void vbat_update()
{
		// --- 1. Sensing (if available) ---
#ifdef HAS_VBAT_SENSING
	vbat_sense_tick();
#endif

#ifdef HAS_VBAT_FAILSAFE
	// --- 2. Re-arm FAILSAFE_VBAT (proof-of-life for the Failsafe aggregator) ---
	// low voltage => FAILSAFE_VBAT = true (HIGH = fault per failsafe-by-default).
	// healthy    => FAILSAFE_VBAT = false (LOW = healthy).
	// The aggregator consumes this each cycle (see failsafe.cb + WIP §5).
	combus_set_digital(comBus, DigitalComBusID::FAILSAFE_VBAT, vbat_is_low(0));
#endif

		// --- 3. (future: ComBus runlevel) ---

		// --- 4. Alert reactions ---
	vbat_alert_tick();
}

#endif // HAS_VBAT_SENSING || VBAT_ALERT

// EOF vbat.cpp
