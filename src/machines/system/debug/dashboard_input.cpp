/******************************************************************************
 * @file dashboard_input.cpp
 * @brief ANSI terminal dashboard � Layer 3 inputs/combus module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_input.h"
#include <core/config/machines/combus_types.h>  // DigitalComBusID, AnalogComBusID (via machine dispatch)
#include <core/system/debug/dashboard.h>
#include <core/system/input/input_manager.h>

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

static const ComBus* s_bus        = nullptr;
static uint8_t       s_analogCh   = 0;
static uint8_t       s_digitalCh  = 0;


// =============================================================================
// 2. VIEW RENDERER
// =============================================================================

/**
 * @brief Render digital channels 4 per row: #N name:val* (DRV asterisk).
 *
 * @details Keeps the table height proportional to digitalCh / 4 instead of
 *   one row per channel, so the view stays within a normal terminal height.
 */
static void renderDigitalCompact(const ComBus* bus, uint8_t n)
{
	for (uint8_t r = 0u; r < n; r += 4u) {
		char buf[DashInnerW + 4];
		int  p = snprintf(buf, sizeof(buf), "  ");
		for (uint8_t j = r; j < r + 4u && j < n; ++j) {
			const char* nm  = bus->digitalBus[j].infoName ? bus->digitalBus[j].infoName : "?";
			const char* val = bus->digitalBus[j].value    ? "ON " : "off";
			const char  drv = bus->digitalBus[j].isDrived ? '*' : ' ';
			p += snprintf(buf + p, sizeof(buf) - (size_t)p,
			              "#%-2u %-14.14s:%-3s%c  ", j, nm, val, drv);
		}
		dLine("%s", buf);
	}
}

/**
 * @brief Render the inputs view: analog table (top) + digital table + combus state (bottom).
 */
static void render_input_view() {
	if (!s_bus) return;

	char upt[12];
	fmtUptime(upt, sizeof(upt));

		// --- Live section header ---
	dPre();
	dTop();
	{
		char left[72], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ INPUTS ]  %s", input_get_name());
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();

		// --- Analog channels ---
	dLine("  %-3s  %-40s  %-6s  %-5s  %s",
	      "CH", "Name", "Raw", "Pct", "Drived");
	dMid();
	for (uint8_t i = 0; i < s_analogCh; i++) {
		uint16_t    raw  = s_bus->analogBus[i].value;
		int16_t     pct  = dashPctBipolar(raw, s_bus->analogBusMaxVal);
		const char* name = s_bus->analogBus[i].infoName ? s_bus->analogBus[i].infoName : "?";
		dLine("  %2u   %-40.40s  %5u  %+4d%%  %s",
		      i, name, raw, pct,
		      s_bus->analogBus[i].isDrived ? "yes" : "no");
	}

		// --- Digital channels ---
	dMid();
	dLine("  digital channels  (* = driven)");
	dMid();
	renderDigitalCompact(s_bus, s_digitalCh);

		// --- ComBus state ---
	dMid();
	dLine("  combus state");
	dMid();
	dLine("  RunLevel: %-14s  keyOn: %-5s  battLow: %-5s  analogBusMax: %-7u  %u analog + %u digital ch",
	      dashRunLevelStr(s_bus->runLevel),
	      s_bus->digitalBus[static_cast<uint8_t>(DigitalComBusID::KEY_ACTIVE)].value ? "YES" : "NO",
	      s_bus->batteryIsLow ? "YES" : "NO",
	      (unsigned)s_bus->analogBusMaxVal, s_analogCh, s_digitalCh);

	dBot();
}


// =============================================================================
// 3. REGISTRATION
// =============================================================================

void dashboard_input_register(const ComBus* bus, uint8_t analogCh, uint8_t digitalCh) {
	s_bus       = bus;
	s_analogCh  = analogCh;
	s_digitalCh = digitalCh;
	dashboard_register_slot('2', "inputs", render_input_view);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_input.cpp
