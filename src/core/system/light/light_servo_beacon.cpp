/******************************************************************************
 * @file light_servo_beacon.cpp
 * @brief Servo-driven beacon protocol update (light side).
 *****************************************************************************/

#include "light_servo_beacon.h"

#include <Arduino.h>
#include <defs/machines_defs.h>

#if defined(SOUND_NODE)
#include <sound_module/init/hw_init_srv.h>
#include <sound_module/config/boards/sound_board_esp32.h>
#endif


// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

#if defined(SOUND_NODE) && defined(SERVO_OUTPUTS_ENABLED)

/**
 * @brief Beacon pulse stepper. Alternates CH3 between min/max angles.
 *
 * @details Each full min->max->min cycle counts as one pulse.
 *   The dwell timing is imposed by beacon hardware protocol.
 *
 * @param pulses  Number of full cycles to output before returning true.
 * @return true once the pulse count has been emitted.
 */
static bool beacon_control_step(uint8_t pulses)
{
	static unsigned long s_lastMs = 0;
	static bool          s_atMax  = false;
	static uint8_t       s_count  = 0;

	if (!srvDevObj) return false;
	if (millis() - s_lastMs >= SrvBeaconDwellMs) {
		s_lastMs = millis();
		s_atMax  = !s_atMax;
		if (!s_atMax) s_count++;

		const SrvHwAngle& hw = srvDevArray[SRV_CH3].hwAngle;
		srvDevObj[SRV_CH3].goToAngle(s_atMax ? hw.maxHwAngle : hw.minHwAngle);
	}

	if (s_count >= pulses) {
		s_count = 0;
		return true;
	}
	return false;
}

#endif // SOUND_NODE && SERVO_OUTPUTS_ENABLED


// =============================================================================
// 2. API
// =============================================================================

void light_servo_beacon_update(bool blueLightOn)
{
#if defined(SOUND_NODE) && defined(SERVO_OUTPUTS_ENABLED)
	if (!srvDevObj) return;
	if (!srvDevArray[SRV_CH3].usage.has_value()) return;

	const DevUsage usage = static_cast<DevUsage>(srvDevArray[SRV_CH3].usage.value());
	if (usage != DevUsage::SIG_BEACON) return;

	static bool s_beaconInit   = false;
	static bool s_lockRotating = false;
	static bool s_lockOff      = false;

		// --- 1. One-time init pulse train (hardware off state sync) ---
	if (!s_beaconInit) {
		if (beacon_control_step(5)) s_beaconInit = true;
		return;
	}

		// --- 2. Running state transitions ---
	if (blueLightOn && !s_lockRotating) {
		if (beacon_control_step(1)) {
			s_lockRotating = true;
			s_lockOff      = false;
		}
	}

	if (!blueLightOn && !s_lockOff && s_lockRotating) {
		if (beacon_control_step(4)) {
			s_lockOff      = true;
			s_lockRotating = false;
		}
	}
#else
	(void)blueLightOn;
#endif
}


// EOF light_servo_beacon.cpp
