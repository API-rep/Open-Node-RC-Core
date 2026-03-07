/******************************************************************************
 * @file dashboard_vbat.cpp
 * @brief ANSI terminal dashboard — Layer 3 battery sensing module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_vbat.h"
#include <core/utils/debug/dashboard.h>
#include <core/utils/vbat/vbat_sense.h>

#include <Arduino.h>
#include <stdio.h>


// =============================================================================
// 1. VIEW RENDERER
// =============================================================================

/**
 * @brief Render the battery view: live voltage channels.
 */
static void render_vbat_view() {
	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[48], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ BATTERY ]  %s", vbat_tech_name());
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();

	uint8_t count = vbat_channel_count();
	if (count == 0) {
		dEmpty();
		dLine("  VBAT_SENSING not active \u2014 no channels configured.");
		dEmpty();
	} else {
			// Header: %-9s "Voltage :" (9 chars) aligns with data %5.2f V (7 chars) + 2 spaces.
			// Cells column starts at position 44 in both header and data rows.
		dLine("  %-3s  %-24s  %-9s  %-5s  %s", "Ch", "Name", "Voltage", "Cells", "Status");
		dMid();
		for (uint8_t i = 0; i < count; i++) {
			const char* name  = vbat_name(i);
			bool        dis   = vbat_is_disabled(i);
			float       v     = dis ? 0.0f : vbat_voltage(i);
			uint8_t     cells = dis ? 0    : vbat_cells(i);
			bool        low   = !dis && vbat_is_low(i);
				// Cells: centered in 5 chars — " nS  " (e.g. " 3S  ") or "  -  " (disabled/unknown)
				// "Cells" header is 5 chars → automatic visual center alignment
			char cellStr[8];
			if (!dis && cells > 0) snprintf(cellStr, sizeof(cellStr), " %uS  ", cells);
			else                   snprintf(cellStr, sizeof(cellStr), "  -  ");
				// Voltage: %5.2f V (enabled) or   --- V (disabled) — V column always aligned
			char prefix[80];
			int  pLen;
			if (dis) pLen = snprintf(prefix, sizeof(prefix), "  %2u   %-24.24s  %5s V    %-5s  ", i, name, "---", cellStr);
			else     pLen = snprintf(prefix, sizeof(prefix), "  %2u   %-24.24s  %5.2f V    %-5s  ", i, name, v, cellStr);
				// Status: centered in 6 chars under "Status" header (len 6)
				//   "  OK" = 2 leading (floor((6-2)/2)), " LOW" = 1 leading (floor((6-3)/2))
			const char* statusAnsi;
			int         statusVis;
			if (dis)       { statusAnsi = " \x1B[33mN/C\x1B[0m"; statusVis = 4; }
			else if (low)  { statusAnsi = " \x1B[31mLOW\x1B[0m"; statusVis = 4; }
			else           { statusAnsi = "  OK";                  statusVis = 4; }
			char row[DashInnerW + 40];
			snprintf(row, sizeof(row), "%s%s", prefix, statusAnsi);
			dContentAnsi(row, pLen + statusVis);
		}
	}

	dBot();
}


// =============================================================================
// 2. DETAIL VIEW RENDERER
// =============================================================================

/**
 * @brief Render the battery detail view for one channel: live state + config at init.
 */
static void render_vbat_detail() {
	uint8_t     ch   = dashboard_detail_index();
	const char* name = vbat_name(ch);
	bool        dis  = vbat_is_disabled(ch);
	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
			// Title: "  [ BATTERY ]  ChX - <name> detail" (left) + "uptime: HH:MM:SS  " (right)
		char left[64], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ BATTERY ]  Ch%u - %s detail", ch, name);
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();

		// ── LIVE STATE ── unified row (same layout for connected and disabled channels)
	{
			// Voltage field
			//   normal:           " 4.06 V"       (7 chars)
			//   drifted from init: " 4.06 V (3.97)" (variable)
			//   disabled:         "  --- V"        (7 chars)
		char voltStr[24];
		if (dis) {
			snprintf(voltStr, sizeof(voltStr), "  --- V");
		} else {
			float v     = vbat_voltage(ch);
			float vInit = vbat_voltage_at_init(ch);
			if (fabsf(v - vInit) > 0.05f)
				snprintf(voltStr, sizeof(voltStr), "%5.2f V (%.2f)", v, vInit);
			else
				snprintf(voltStr, sizeof(voltStr), "%5.2f V", v);
		}
			// Type field:  "<tech> - <cells>S" or "<tech> - --" (disabled)
		char typeStr[16];
		if (dis) {
			snprintf(typeStr, sizeof(typeStr), "%s - --", vbat_tech_name());
		} else {
			uint8_t cells = vbat_cells(ch);
			if (cells > 0) snprintf(typeStr, sizeof(typeStr), "%s - %uS", vbat_tech_name(), cells);
			else           snprintf(typeStr, sizeof(typeStr), "%s - ?",  vbat_tech_name());
		}
			// Status field
		const char* statusAnsi;
		int         statusVis;
		if (dis)                  { statusAnsi = "\x1B[33mN/C\x1B[0m"; statusVis = 3; }  // yellow
		else if (vbat_is_low(ch)) { statusAnsi = "\x1B[31mLOW\x1B[0m"; statusVis = 3; }  // red
		else                      { statusAnsi = "OK";                  statusVis = 2; }
			// Assemble:  Voltage :  X.XX V  |  Type :  TECH - NS  |  Status :  <status>
		char prefix[80];
		int  pLen = snprintf(prefix, sizeof(prefix),
		                     "  Voltage :  %s  |  Type :  %s  |  Status :  ",
		                     voltStr, typeStr);
		char row[DashInnerW + 40];
		snprintf(row, sizeof(row), "%s%s", prefix, statusAnsi);
		dContentAnsi(row, pLen + statusVis);
	}
	dMid();

		// ── CONFIG AT INIT ──
	const VBatSenseConfig* cfg = vbat_cfg(ch);
	if (cfg) {
		dLine("  Sensing pin :  GPIO %u at ADC ref. %.2f V",
		      cfg->pin, cfg->adcRefVoltage);
		dLine("  High/low-side resistor :  %lu / %lu Ohm",
		      (unsigned long)cfg->hsResOhm, (unsigned long)cfg->lsResOhm);
		dLine("  Diode drop :  %.2f V",
		      cfg->diodeDrop);
		dLine("  Cutoff/charged cell voltage :  %.2f / %.2f V/cell",
		      cfg->cutoffVolt, cfg->chargedVolt);
		dLine("  Recover hysteresis :  %.2f V",
		      cfg->hysteresis);
		dLine("  Refresh tick :  %lu ms",
		      (unsigned long)cfg->intervalMs);
	} else {
		dLine("  (config not available)");
	}

	dBot();
}


// =============================================================================
// 3. REGISTRATION
// =============================================================================

void dashboard_vbat_register() {
	dashboard_register_slot('4', "battery", render_vbat_view);
	dashboard_register_detail('4', vbat_channel_count, render_vbat_detail);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_vbat.cpp
