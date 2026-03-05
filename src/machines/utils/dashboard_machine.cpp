/******************************************************************************
 * @file dashboard_machine.cpp
 * @brief ANSI terminal dashboard — Layer 2 machine environment.
 *
 * @details Owns the overview view (slot '0'), stores machine data pointers,
 *   and orchestrates registration of all Layer 3 module slots.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_machine.h"
#include "dashboard_drv.h"
#include "dashboard_input.h"
#include "dashboard_vbat.h"
#include <core/utils/debug/dashboard.h>
#include <core/utils/vbat/vbat_sense.h>

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

static const ComBus*  s_bus       = nullptr;
static const Machine* s_mach      = nullptr;
static uint8_t        s_analogCh  = 0;
static uint8_t        s_digitalCh = 0;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/** @brief Map a CombusLayout to a human-readable string. */
static const char* combusLayoutStr(CombusLayout layout) {
	switch (layout) {
		case CombusLayout::DUMPER_TRUCK: return "DUMPER_TRUCK";
		case CombusLayout::UNDEFINED:    return "UNDEFINED";
		default:                         return "---";
	}
}


// =============================================================================
// 3. OVERVIEW VIEW
// =============================================================================

/**
 * @brief Render view 0: Overview.
 *
 * @details Header row (name / runlevel / uptime), one-line status bar
 *   (battery / controller / drivers / key), per-channel input summary,
 *   and the event ring buffer.
 */
static void render_overview() {
	if (!s_bus || !s_mach) return;

	char upt[12];
	fmtUptime(upt, sizeof(upt));

		// --- 1. Header ---
	dTop();
	dLine("  %-38s %-12s %s",
		s_mach->infoName, dashRunLevelStr(s_bus->runLevel), upt);

		// --- 2. Status bar ---
	dMid();

	char batStr[16];
	if (vbat_channel_count() > 0) {
		snprintf(batStr, sizeof(batStr), "%.2fV %s",
			vbat_voltage(0), vbat_is_low(0) ? "LOW" : "OK ");
	} else {
		snprintf(batStr, sizeof(batStr), "N/A    ");
	}

	bool ctrlConn = false;
	for (uint8_t i = 0; i < s_analogCh; i++) {
		if (s_bus->analogBus[i].isDrived) { ctrlConn = true; break; }
	}

	bool drvEnabled = (s_bus->runLevel == RunLevel::RUNNING ||
	                   s_bus->runLevel == RunLevel::STARTING);

	dLine("  BAT: %-8s  CTRL: %-4s  DRV: %d/%d %-3s  KEY: %-3s",
		batStr,
		ctrlConn ? "CONN" : "DISC",
		s_mach->dcDevCount, s_mach->dcDevCount, drvEnabled ? "ENB" : "DIS",
		s_bus->keyOn ? "ON" : "OFF"
	);

		// --- 3. Channel summary ---
	dMid();
	for (uint8_t i = 0; i < s_analogCh; i++) {
		uint16_t raw = s_bus->analogBus[i].value;
		int16_t  pct = dashPctBipolar(raw, s_bus->analogBusMaxVal);
		bool     drv = s_bus->analogBus[i].isDrived;
		dLine("  %-24.24s  %5u  %+4d%%  %s",
			s_bus->analogBus[i].infoName, raw, pct,
			drv ? "DRV" : "---"
		);
	}
	for (uint8_t i = 0; i < s_digitalCh; i++) {
		bool val = s_bus->digitalBus[i].value;
		bool drv = s_bus->digitalBus[i].isDrived;
		dLine("  %-24.24s  %-4s          %s",
			s_bus->digitalBus[i].infoName,
			val ? "ON" : "OFF",
			drv ? "DRV" : "---"
		);
	}

		// --- 4. Event log ---
	dMid();
	dashboard_render_events();

	dBot();
}


// =============================================================================
// 4. PUBLIC API
// =============================================================================

/**
 * @brief Full dashboard stack setup — single entry point from machine init.
 *
 * @details Resets core state, registers the overview slot and all module
 *   slots, then pushes an initial event.
 *
 * @param bus        Pointer to the active ComBus instance.
 * @param mach       Pointer to the Machine config.
 * @param analogCh   Total number of analog combus channels.
 * @param digitalCh  Total number of digital combus channels.
 */
void dashboard_machine_setup(const ComBus* bus, const Machine* mach,
                              uint8_t analogCh, uint8_t digitalCh) {
		// --- 1. Store machine data ---
	s_bus       = bus;
	s_mach      = mach;
	s_analogCh  = analogCh;
	s_digitalCh = digitalCh;

		// --- 2. Reset core dashboard state ---
	dashboard_setup();

		// --- 3. Register overview slot (always key '0', always first) ---
	dashboard_register_slot('0', "overview", render_overview);

		// --- 4. Register module slots ---
	dashboard_input_register(bus, analogCh, digitalCh);
	dashboard_drv_register(bus, mach);
	dashboard_vbat_register();

		// --- 5. Initial event marker ---
	dashboard_push_event("dashboard ready");
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_machine.cpp
