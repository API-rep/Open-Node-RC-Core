/******************************************************************************
 * @file  vbat.cpp
 * @brief Battery management: the entry point of the battery modules.
 *
 * @details Sequences vbat_sense and vbat_alert, each under its own flag.  Without
 *   HAS_VBAT_SENSING, the sensing steps are skipped and `sense` may be nullptr.
 *
 *   VBAT_FAIL = vbat_is_low(): true when the battery is low, false when it is
 *   healthy.  The hard chain reads it, then puts it back to fault until the next
 *   cycle.
 *****************************************************************************/

#include "vbat.h"

	// config.h first: it defines HAS_VBAT_SENSING, tested below
#include <core/config/vbat/config.h>

#if defined(HAS_VBAT_SENSING) || defined(HAS_VBAT_FAILSAFE)
#include <core/config/machines/combus_types.h>   // DigitalComBusID::VBAT_FAIL
#include <core/system/combus/combus_access.h>    // combus_set_digital()
#endif

#if defined(HAS_VBAT_SENSING) || defined(VBAT_ALERT)



// =============================================================================
// 1. INIT
// =============================================================================

/**
 * @brief Battery init. The sensing, then the alert hardware.
 *
 * @details In order:
 *   1. Sensing, when the board has it (@p sense non-null and HAS_VBAT_SENSING): ADC
 *      setup, sliding average seed, cell count detection, first low battery state.
 *   2. Alert hardware (buzzer channel...).
 */

void vbat_init(VBatSense* sense)
{
		// 1. Sensing
#ifdef HAS_VBAT_SENSING
	if (sense) {
		vbat_sense_init(*sense);
	}
#endif

		// 2. Alert hardware
	vbat_alert_init();
}



// =============================================================================
// 2. STEP
// =============================================================================

/**
 * @brief Battery step. Sensing, then the contributor, then the alert.
 *
 * @details In order:
 *   1. Sensing: ADC read, sliding average, low battery detection.
 *   2. VBAT_FAIL from vbat_is_low(0), with HAS_VBAT_FAILSAFE: written every cycle,
 *      so that the contributor is fresh when the hard chain reads it.
 *   3. Alert reactions.
 */

void vbat_update(ComBus& bus)
{
		// 1. Sensing
#ifdef HAS_VBAT_SENSING
	vbat_sense_tick();
#endif

		// 2. Contributor
#ifdef HAS_VBAT_FAILSAFE
	combus_set_digital(bus, DigitalComBusID::VBAT_FAIL, vbat_is_low(0));
#else
	(void)bus;
#endif

		// 3. Alert
	vbat_alert_tick();
}

#endif // HAS_VBAT_SENSING || VBAT_ALERT

// EOF vbat.cpp
