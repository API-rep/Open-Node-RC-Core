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
#include "dashboard_sig.h"
#include "dashboard_simulation.h"
#include <core/system/debug/dashboard.h>
#include <core/system/vbat/vbat_sense.h>
#include <struct/simulation_struct.h>   // DriveStateBus

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

static const ComBus*  s_bus       = nullptr;
static const EnvCfg* s_mach      = nullptr;
static uint8_t        s_analogCh  = 0;
static uint8_t        s_digitalCh = 0;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

/**
 * @brief Render digital channels 4 per row: #N name:val* (DRV asterisk).
 *
 * @details Reduces the digital channel section from one row per channel to
 *   one row per four channels, keeping the overview within terminal height.
 */
static void renderDigitalSummary(const ComBus* bus, uint8_t n)
{
	uint8_t activeCount = 0;
	char    activeNames[80] = "";
	int     p = 0;

	for (uint8_t i = 0; i < n; i++) {
		if (bus->digitalBus[i].value) {
			activeCount++;
			if (activeCount <= 4 && p < 60) {  // max 4 names, leave room
				const char* nm = bus->digitalBus[i].infoName ? bus->digitalBus[i].infoName : "?";
				if (activeCount > 1) p += snprintf(activeNames + p, sizeof(activeNames) - (size_t)p, ", ");
				p += snprintf(activeNames + p, sizeof(activeNames) - (size_t)p, "%s", nm);
			}
		}
	}
	if (activeCount > 4) snprintf(activeNames + p, sizeof(activeNames) - (size_t)p, ", ...");

	if (activeCount > 0) {
		dLine("  Digital: %u/%u active  (%s)", activeCount, n, activeNames);
	} else {
		dLine("  Digital: 0/%u active", n);
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
	dPre();
	dTop();
	{
		char left[56], right[36];
		int lLen = snprintf(left,  sizeof(left),  "  [ OVERVIEW ]  %s", s_mach->infoName);
		int rLen = snprintf(right, sizeof(right), "%-12s  uptime: %s  ", dashRunLevelStr(s_bus->runLevel), upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}

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
	for (uint8_t i = 0; i < s_analogCh && !ctrlConn; i++) {
		if (s_bus->analogBus[i].isDrived)  ctrlConn = true;
	}
	for (uint8_t i = 0; i < s_digitalCh && !ctrlConn; i++) {
		if (s_bus->digitalBus[i].isDrived) ctrlConn = true;
	}

	bool drvEnabled = (s_bus->runLevel == RunLevel::RUNNING ||
	                   s_bus->runLevel == RunLevel::STARTING);
	const bool keyActive = s_bus->digitalBus[static_cast<uint8_t>(DigitalComBusID::KEY_ACTIVE)].value;

	dLine("  BAT: %-8s  CTRL: %-4s  DRV: %-3s (%u dev)  KEY: %-3s",
		batStr,
		ctrlConn ? "CONN" : "DISC",
		drvEnabled ? "ENB" : "DIS", s_mach->dcDevCount,
		keyActive ? "ON" : "OFF"
	);

		// --- 3. Channel summary (wire channels only: 0..WIRE_END-1) ---
	dMid();
	const uint8_t wireEnd = static_cast<uint8_t>(AnalogComBusID::WIRE_END);
	for (uint8_t i = 0; i < wireEnd && i < s_analogCh; i++) {
		uint16_t    raw  = s_bus->analogBus[i].value;
		int16_t     pct  = dashPctBipolar(raw, s_bus->analogBusMaxVal);
		bool        drv  = s_bus->analogBus[i].isDrived;
		const char* name = s_bus->analogBus[i].infoName ? s_bus->analogBus[i].infoName : "?";

		// Special decoding for DRIVE_STATE_BUS (index 5) — display readable text
		if (i == static_cast<uint8_t>(AnalogComBusID::DRIVE_STATE_BUS)) {
			const int8_t ds = DriveStateBus::decode(raw);
			const char* stateStr;
			switch (ds) {
				case -2: stateStr = "BRAKE_REV"; break;  // kBrakeRev
				case -1: stateStr = "REV";       break;  // kDriveRev
				case  0: stateStr = "STAND";     break;  // kStanding
				case +1: stateStr = "BRAKE_FWD"; break;  // kBrakeFwd
				case +2: stateStr = "FWD";       break;  // kDriveFwd
				default: stateStr = "???";       break;
			}
			dLine("  %2u  %-44.44s  %-10s       %s",
				i, name, stateStr, drv ? "DRV" : "---");
		} else {
			dLine("  %2u  %-44.44s  %5u  %+4d%%  %s",
				i, name, raw, pct, drv ? "DRV" : "---");
		}
	}
	if (s_digitalCh > 0u) {
		dMid();
		renderDigitalSummary(s_bus, s_digitalCh);
	}

		// --- 4. Serial log tail ---
	dMid();
	dLine("  recent log :");
	dashboard_render_log();

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
 * @param mach       Pointer to the EnvCfg config.
 * @param analogCh   Total number of analog combus channels.
 * @param digitalCh  Total number of digital combus channels.
 */
void dashboard_machine_setup(const ComBus* bus, const EnvCfg* mach,
                              uint8_t analogCh, uint8_t digitalCh) {
		// --- 1. Store machine data ---
	s_bus       = bus;
	s_mach      = mach;
	s_analogCh  = analogCh;
	s_digitalCh = digitalCh;

		// --- 2. Reset core dashboard state ---
	dashboard_setup();

		// --- 3. Register overview slot (always key '1', always first) ---
	dashboard_register_slot('1', "overview", render_overview);

		// --- 4. Register module slots ---
	dashboard_input_register(bus, analogCh, digitalCh);
	dashboard_drv_register(bus, mach);
	dashboard_vbat_register();
	dashboard_sig_register(bus, mach);
	dashboard_simulation_register(bus, mach);

		// NOTE: dashboard_start_task() is NOT called here.
		// It must be called from init.cpp *after* the PAUSE_LOG_AFTER_INIT
		// block so the dashboard task does not activate during the pause.

}


#endif // DEBUG_DASHBOARD

// EOF dashboard_machine.cpp
