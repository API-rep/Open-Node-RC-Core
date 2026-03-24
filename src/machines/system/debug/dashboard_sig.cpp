/******************************************************************************
 * @file dashboard_sig.cpp
 * @brief ANSI terminal dashboard — Layer 3 signal device module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_sig.h"
#include <core/system/debug/dashboard.h>

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

static const ComBus*  s_bus  = nullptr;
static const Machine* s_mach = nullptr;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

static const char* devUsageStr(DevUsage u) {
	switch (u) {
		case DevUsage::TRACT_WHEEL:   return "Tract.Wheel";
		case DevUsage::TRACT_TRACK:   return "Tract.Track";
		case DevUsage::HYD_LINEAR:    return "Hyd.Linear";
		case DevUsage::HYD_ROTARY:    return "Hyd.Rotary";
		case DevUsage::HYD_ASSIST:    return "Hyd.Assist";
		case DevUsage::HYD_PUMP:      return "Hyd.Pump";
		case DevUsage::STEER_SERVO:   return "Steer.Servo";
		case DevUsage::STEER_MOTOR:   return "Steer.Motor";
		case DevUsage::SIG_HORN:      return "Sig.Horn";
		case DevUsage::SIG_LIGHT:     return "Sig.Light";
		case DevUsage::SIG_SOLENOID:  return "Sig.Solenoid";
		case DevUsage::UNDEFINED: default: return "---";
	}
}

/**
 * @brief Fill @p out with the channel descriptor string ("D:N" or "A:N").
 */
static void chanFmt(const SigDevice& d, char* out, size_t sz) {
	if (d.digitalChannel.has_value())
		snprintf(out, sz, "D:%-2d", static_cast<int>(d.digitalChannel.value()));
	else if (d.analogChannel.has_value())
		snprintf(out, sz, "A:%-2d", static_cast<int>(d.analogChannel.value()));
	else
		snprintf(out, sz, "---");
}

/**
 * @brief Return a live value string for @p d.
 *   Digital channel → "ON"/"OFF"; analog channel → "NNN%"; no channel → "---".
 */
static void sigValFmt(const SigDevice& d, char* out, size_t sz) {
	if (d.digitalChannel.has_value()) {
		uint8_t ch = static_cast<uint8_t>(d.digitalChannel.value());
		snprintf(out, sz, "%s", s_bus->digitalBus[ch].value ? "ON " : "OFF");
	} else if (d.analogChannel.has_value()) {
		uint8_t ch  = static_cast<uint8_t>(d.analogChannel.value());
		int16_t pct = dashPctOneway(s_bus->analogBus[ch].value, s_bus->analogBusMaxVal);
		snprintf(out, sz, "%3d%%", pct);
	} else {
		snprintf(out, sz, "---");
	}
}

/**
 * @brief Return true if the ComBus channel backing @p d is presently driven.
 */
static bool sigIsDriven(const SigDevice& d) {
	if (d.digitalChannel.has_value())
		return s_bus->digitalBus[static_cast<uint8_t>(d.digitalChannel.value())].isDrived;
	if (d.analogChannel.has_value())
		return s_bus->analogBus[static_cast<uint8_t>(d.analogChannel.value())].isDrived;
	return false;
}


// =============================================================================
// 3. VIEW RENDERER
// =============================================================================

/**
 * @brief Render the signal-device slot view: live ComBus values table.
 */
static void render_sig_view() {
	if (!s_bus || !s_mach) return;

	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[56], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ SIGNALS ]  %u devices configured",
		                    (unsigned)s_mach->sigDevCount);
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();
	dLine("  RunLevel: %-12s  Sig devices: %u",
	      dashRunLevelStr(s_bus->runLevel), (unsigned)s_mach->sigDevCount);
	dMid();
	dLine("  %-2s  %-32s  %-12s  %-6s  %-5s  %s",
	      "ID", "Name", "Usage", "Chan", "Value", "Drv");
	dMid();

	if (!s_mach->sigDev || s_mach->sigDevCount == 0) {
		dLine("  (no signal devices configured)");
	} else {
		for (uint8_t i = 0; i < s_mach->sigDevCount; i++) {
			const SigDevice& d    = s_mach->sigDev[i];
			const char*      name = d.infoName ? d.infoName : "?";
			char chBuf[8], valBuf[8];
			chanFmt(d, chBuf, sizeof(chBuf));
			sigValFmt(d, valBuf, sizeof(valBuf));
			dLine("  %2d  %-32.32s  %-12.12s  %-6s  %-5s  %s",
			      d.ID, name, devUsageStr(d.usage), chBuf, valBuf,
			      sigIsDriven(d) ? "DRV" : "---");
		}
	}

	dBot();
}


// =============================================================================
// 4. DETAIL VIEW RENDERER
// =============================================================================

/**
 * @brief Return the number of signal devices (used as detail item count).
 */
static uint8_t sig_detail_count() {
	return s_mach ? s_mach->sigDevCount : 0;
}

/**
 * @brief Render the per-signal detail view: live state + config at init.
 */
static void render_sig_detail() {
	if (!s_bus || !s_mach) return;
	uint8_t idx = dashboard_detail_index();
	if (idx >= s_mach->sigDevCount) return;

	const SigDevice& d    = s_mach->sigDev[idx];
	const char*      name = d.infoName ? d.infoName : "?";
	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[64], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ SIGNALS ]  #%d - %s", d.ID, name);
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();

		// -- LIVE STATE --
	{
		char valBuf[8];
		sigValFmt(d, valBuf, sizeof(valBuf));
		dLine("  Value :  %-5s  |  Driven :  %s",
		      valBuf, sigIsDriven(d) ? "YES" : "---");
	}
	dMid();

		// -- CONFIG AT INIT --
	{
		char chBuf[8];
		chanFmt(d, chBuf, sizeof(chBuf));

		  // ComBus channel name from the live bus slot
		const char* busName = "---";
		if (d.digitalChannel.has_value()) {
			uint8_t ch = static_cast<uint8_t>(d.digitalChannel.value());
			if (s_bus->digitalBus[ch].infoName) busName = s_bus->digitalBus[ch].infoName;
		} else if (d.analogChannel.has_value()) {
			uint8_t ch = static_cast<uint8_t>(d.analogChannel.value());
			if (s_bus->analogBus[ch].infoName) busName = s_bus->analogBus[ch].infoName;
		}

		dLine("  Usage :  %s",       devUsageStr(d.usage));
		dLine("  Channel :  %s",     chBuf);
		dLine("  ComBus name :  %s", busName);
	}

	dBot();
}


// =============================================================================
// 5. REGISTRATION
// =============================================================================

void dashboard_sig_register(const ComBus* bus, const Machine* mach) {
	s_bus  = bus;
	s_mach = mach;
	dashboard_register_slot('5', "signals", render_sig_view);
	dashboard_register_detail('5', sig_detail_count, render_sig_detail);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_sig.cpp
