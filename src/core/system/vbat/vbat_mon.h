/******************************************************************************
 * @file vbat_mon.h
 * @brief Battery monitoring orchestrator — coordinates sensing and alerts.
 *
 * @details Top-level battery module composed of two independent sub-modules:
 *
 *   - `vbat_sense` — ADC read, sliding average, cell detection, low-bat flag.
 *     Writes `batteryIsLow` into the ComBus digital bus as event.
 *     Activated by `-D VBAT_SENSING=<TYPE>`.
 *
 *   - `vbat_alert` — reactions to a ComBus batteryIsLow event: boot beep,
 *     out-of-fuel WAV, hazard flash.  Each reaction is independently
 *     gated by its own compile flag (`VBAT_ALERT_BEEP`, `VBAT_ALERT_SOUND`,
 *     `VBAT_ALERT_LIGHT`).
 *
 *   Both sub-modules are optional and can operate separately.
 *   The ComBus `batteryIsLow` flag is the pivot: `vbat_sense` (or any
 *   other remote node on the bus) writes it, and `vbat_alert` reads it to
 *   trigger reactions — regardless of who produced the sensing data.
 *   A sound module running standalone senses and reacts locally; a sound
 *   module receiving ComBus frames from a machine reacts to the remote
 *   `batteryIsLow` flag without doing any sensing itself.
 *
 *   ComBus runlevel reaction (sleeping / re-arm) to the `batteryIsLow`
 *   flag is also machine-config dependent and will be gated by a
 *   dedicated compile flag (future `-D VBAT_COMBUS_RUNLEVEL`).
 *   That logic lives at the `vbat_mon` root level, between the sense
 *   and alert steps.
 *
 *   Callers wire application-level hooks once at init and call
 *   `vbat_mon_update()` each loop cycle — single call, full battery
 *   management.
 *
 *   When VBAT_SENSING is not defined, all functions are inline no-ops.
 *****************************************************************************/
#pragma once

#include <core/system/vbat/vbat_alert.h>


#ifdef VBAT_SENSING

// =============================================================================
// 1. API
// =============================================================================

	/// Application-level init: register hooks and run alert boot sequence.
	/// Must be called after vbat_init() (hardware ADC setup in sys_init).
void vbat_mon_init(const VBatAlertHooks& hooks);

	/// Main-loop update: sense tick, then alert tick.
void vbat_mon_update();

#else // VBAT_SENSING not defined

// =============================================================================
// 2. NO-OP STUBS
// =============================================================================

inline void vbat_mon_init(const VBatAlertHooks&) {}
inline void vbat_mon_update() {}

#endif // VBAT_SENSING

// EOF vbat_mon.h
