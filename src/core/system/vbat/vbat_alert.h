/******************************************************************************
 * @file vbat_alert.h
 * @brief Battery low-voltage alert reactions — core module.
 *
 * @details Triggers compile-flag gated reactions when battery is low.
 *   The `comBus.batteryIsLow` flag is the universal pivot — written by
 *   `vbat_update()` from local sensing (VBAT_SENSING) or received from
 *   a ComBus RX frame.  No dependency on app-specific headers.
 *
 *   Compile flags (set in platformio.ini):
 *     -D VBAT_ALERT_BEEP   — boot beep via Tone32 on startup.
 *     -D VBAT_ALERT_SOUND  — (reserved — consumer reads comBus.batteryIsLow).
 *     -D VBAT_ALERT_LIGHT  — (reserved — reaction wired by consumer).
 *
 *   When no VBAT_ALERT_* flag is set, all functions are inline no-ops.
 *****************************************************************************/
#pragma once

#include <core/system/vbat/vbat_sense.h>


// --- Derive umbrella flag from individual alert sub-flags ---
#if defined(VBAT_ALERT_BEEP) || defined(VBAT_ALERT_SOUND) || defined(VBAT_ALERT_LIGHT)
  #ifndef VBAT_ALERT
    #define VBAT_ALERT
  #endif
#endif

#ifdef VBAT_ALERT

// =============================================================================
// 1. API
// =============================================================================

	/// Boot-time: initialize alert hardware (buzzer LEDC channel, etc.).
void vbat_alert_init();

	/// Runtime: read `comBus.batteryIsLow` and trigger gated reactions.
	/// Does NOT call vbat_sense_tick() — orchestrator (vbat) handles that.
void vbat_alert_tick();


// =============================================================================
// 2. ACCESSORS — thin wrappers over vbat_sense + comBus
// =============================================================================

	/// Current battery voltage (V).  Returns 0 in alert-only mode.
inline float vbat_alert_voltage() { return vbat_voltage(0); }

	/// Auto-detected cell count (1S–6S).  Returns 0 in alert-only mode.
inline uint8_t vbat_alert_cells() { return vbat_cells(0); }

	/// True when battery is below cutoff (sensing mode only).
inline bool vbat_alert_active() { return vbat_is_low(0); }

	/// Total cutoff voltage (cutoffVolt * cells).  Returns 0 in alert-only mode.
inline float vbat_alert_cutoff()
{
	const auto* cfg = vbat_cfg(0);
	return cfg ? cfg->cutoffVolt * vbat_cells(0) : 0.0f;
}

#else // no VBAT_ALERT_* flag set

// =============================================================================
// 3. NO-OP STUBS
// =============================================================================

inline void    vbat_alert_init() {}
inline void    vbat_alert_tick()     {}
inline float   vbat_alert_voltage()  { return 0.0f; }
inline uint8_t vbat_alert_cells()    { return 0; }
inline bool    vbat_alert_active()   { return false; }
inline float   vbat_alert_cutoff()   { return 0.0f; }

#endif // VBAT_ALERT

// EOF vbat_alert.h
