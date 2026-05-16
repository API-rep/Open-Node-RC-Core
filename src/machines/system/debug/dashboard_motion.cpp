/******************************************************************************
 * @file dashboard_motion.cpp
 * @brief ANSI terminal dashboard — Layer 3 motion/inertia module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_motion.h"
#include <core/system/debug/dashboard.h>
#include <struct/motion_struct.h>

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

static const ComBus*  s_bus  = nullptr;
static const EnvCfg*  s_mach = nullptr;


// =============================================================================
// 2. PRIVATE HELPERS
// =============================================================================

static const char* driveStateStr(int8_t s) {
	switch (s) {
		case DriveState::kStanding: return "STAND  ";
		case DriveState::kDriveFwd: return "FWD    ";
		case DriveState::kBrakeFwd: return "BRK    ";
		case DriveState::kDriveRev: return "REV    ";
		case DriveState::kBrakeRev: return "BRK    ";
		default:                    return "???    ";
	}
}

/**
 * @brief Count unique ComBus analog channels used by motion-enabled devices.
 *   Devices with no comChannel are grouped under sentinel value 255.
 */
static uint8_t motionChannelCount() {
	if (!s_mach) return 0;
	uint8_t seen[16];
	uint8_t count = 0;
	for (uint8_t i = 0; i < s_mach->dcDevCount; i++) {
		const DcDevice& d = s_mach->dcDev[i];
		if (!d.motion) continue;
		uint8_t ch = d.comChannel.has_value()
		           ? static_cast<uint8_t>(d.comChannel.value()) : 255u;
		bool found = false;
		for (uint8_t j = 0; j < count; j++) {
			if (seen[j] == ch) { found = true; break; }
		}
		if (!found && count < 16) seen[count++] = ch;
	}
	return count;
}

/**
 * @brief Return the first DcDevice* using the @p idx -th unique motion channel.
 *   Channels are ordered by first occurrence in dcDev[].
 */
static const DcDevice* motionChannelAt(uint8_t idx) {
	if (!s_mach) return nullptr;
	uint8_t seen[16];
	uint8_t count = 0;
	for (uint8_t i = 0; i < s_mach->dcDevCount; i++) {
		const DcDevice& d = s_mach->dcDev[i];
		if (!d.motion) continue;
		uint8_t ch = d.comChannel.has_value()
		           ? static_cast<uint8_t>(d.comChannel.value()) : 255u;
		bool found = false;
		for (uint8_t j = 0; j < count; j++) {
			if (seen[j] == ch) { found = true; break; }
		}
		if (!found) {
			if (count == idx) return &d;
			if (count < 16)   seen[count++] = ch;
		}
	}
	return nullptr;
}


// =============================================================================
// 3. MAIN VIEW RENDERER
// =============================================================================

/**
 * @brief Render the motion view: live inertia table per traction device.
 */
static void render_motion_view() {
	if (!s_bus || !s_mach) return;

	char upt[12];
	fmtUptime(upt, sizeof(upt));

	uint8_t chCount = motionChannelCount();

		// --- Header ---
	dPre();
	dTop();
	{
		char left[64], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ MOTION ]  %s", s_mach->infoName);
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();
	dLine("  RunLevel: %-12s  Motion channels: %u",
	      dashRunLevelStr(s_bus->runLevel), chCount);
	dMid();
	dLine("  %-2s  %-24s  %-11s  %-5s  %-7s  %s",
	      "Ch", "Channel", "raw %", "speed", "State", "Gear");
	dMid();

	for (uint8_t ci = 0; ci < chCount; ci++) {
		const DcDevice* dp = motionChannelAt(ci);
		if (!dp) continue;
		const MotionRuntime& rt = dp->motionRt;

		char rawPctStr[14], speedStr[7], chStr[4];
		const char* chanName = "?";
		if (dp->comChannel.has_value()) {
			uint8_t  ch       = static_cast<uint8_t>(dp->comChannel.value());
			uint16_t raw      = s_bus->analogBus[ch].value;
			int16_t  rawPct   = dashPctBipolar(raw,           s_bus->analogBusMaxVal);
			int16_t  speedPct = dashPctBipolar(rt.currentPos, s_bus->analogBusMaxVal);
			snprintf(rawPctStr, sizeof(rawPctStr), "%5u %+4d%%", raw, (int)rawPct);
			snprintf(speedStr,  sizeof(speedStr),  "%+4d%%", (int)speedPct);
			snprintf(chStr,     sizeof(chStr),     "%2u", ch);
			if (s_bus->analogBus[ch].infoName) chanName = s_bus->analogBus[ch].infoName;
		} else {
			snprintf(rawPctStr, sizeof(rawPctStr), "  ---      ");
			snprintf(speedStr,  sizeof(speedStr),  "  ---");
			snprintf(chStr,     sizeof(chStr),     "--");
			if (dp->infoName) chanName = dp->infoName;
		}

		dLine("  %s  %-24.24s  %-11s  %-5s  %-7s  %d",
		      chStr, chanName, rawPctStr, speedStr,
		      driveStateStr(rt.driveState), (int)rt.gearSetTo);
	}

	dBot();
}


// =============================================================================
// 4. DETAIL VIEW
// =============================================================================

/** @brief Return the number of unique motion channels (detail item count). */
static uint8_t motion_detail_count() {
	return motionChannelCount();
}

/**
 * @brief Render per-device detail: live runtime state + motion config presets.
 *
 * @details Fixed layout — always 7 content rows (1 live + 6 config), regardless
 *   of which config sub-pointers are null.  Null sections show "N/A" placeholder
 *   rows to keep frame height constant while navigating between devices.
 */
static void render_motion_detail() {
	if (!s_bus || !s_mach) return;
	const DcDevice* dp = motionChannelAt(dashboard_detail_index());
	if (!dp) return;
	const DcDevice&      d   = *dp;
	const MotionRuntime& rt  = d.motionRt;
	const MotionConfig*  cfg = d.motion;

	uint16_t    rawVal   = 0;
	uint8_t     chNum    = 255u;
	const char* chanName = "no channel";
	if (d.comChannel.has_value()) {
		chNum  = static_cast<uint8_t>(d.comChannel.value());
		rawVal = s_bus->analogBus[chNum].value;
		if (s_bus->analogBus[chNum].infoName) chanName = s_bus->analogBus[chNum].infoName;
	}
	int16_t rawPct   = dashPctBipolar(rawVal,         s_bus->analogBusMaxVal);
	int16_t speedPct = dashPctBipolar(rt.currentPos,  s_bus->analogBusMaxVal);

	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[72], right[28];
		int lLen = (chNum != 255u)
		         ? snprintf(left, sizeof(left), "  [ MOTION DETAIL ]  Ch%u - %s", chNum, chanName)
		         : snprintf(left, sizeof(left), "  [ MOTION DETAIL ]  %s", chanName);
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();

		// --- Live row (row 1): raw input + motion-managed speed + state + gear ---
	dLine("  raw : %5u %+4d%%  |  speed : %+4d%%  |  state : %-7s  |  gear : %d   (live)",
	      rawVal, (int)rawPct, (int)speedPct, driveStateStr(rt.driveState), (int)rt.gearSetTo);

	dMidLabel("config");

		// --- Gear timing (rows 2–3) ---
	if (cfg->gear) {
		uint16_t coast = cfg->gear->coastRampMs  ? cfg->gear->coastRampMs  : cfg->gear->rampTimeThirdMs;
		uint16_t brake = cfg->gear->brakeRampMs  ? cfg->gear->brakeRampMs  : cfg->gear->rampTimeFirstMs;
		dLine("  Gear 1st : %4u ms   |  Gear 2nd : %4u ms   |  Gear 3rd : %4u ms",
		      cfg->gear->rampTimeFirstMs, cfg->gear->rampTimeSecondMs, cfg->gear->rampTimeThirdMs);
		dLine("  coast    : %4u ms   |  brake    : %4u ms   |  scale    :  %3u %%",
		      coast, brake, cfg->gear->globalAccelPct);
	} else {
		dLine("  gear     :  N/A");
		dLine("  ---");
	}

		// --- Inertia model (rows 4–5) ---
	if (cfg->inertia) {
		dLine("  accel    : %5u      |  coast    : %5u      |  brake    : %5u",
		      cfg->inertia->accelSteps, cfg->inertia->coastSteps, cfg->inertia->brakeSteps);
		dLine("  brkMargin: %5u      |  ctrBrake : %5u",
		      cfg->inertia->brakeMargin, cfg->inertia->counterBrakeSteps);
	} else {
		dLine("  inertia  :  N/A");
		dLine("  ---");
	}

		// --- Dead-band (row 6) ---
	if (cfg->band) {
		int16_t lo = dashPctBipolar(cfg->band->minNeutral, s_bus->analogBusMaxVal);
		int16_t hi = dashPctBipolar(cfg->band->maxNeutral, s_bus->analogBusMaxVal);
		dLine("  band     :  %5u .. %5u   (%+d%% .. %+d%%  neutral)",
		      cfg->band->minNeutral, cfg->band->maxNeutral, (int)lo, (int)hi);
	} else {
		dLine("  band     :  N/A");
	}

		// --- Soft margin (row 7) ---
	if (cfg->margin) {
		int16_t minPct = dashPctBipolar(cfg->margin->minVal, s_bus->analogBusMaxVal);
		int16_t maxPct = dashPctBipolar(cfg->margin->maxVal, s_bus->analogBusMaxVal);
		dLine("  margin   :  min %5u (%+d%%)   max %5u (%+d%%)",
		      cfg->margin->minVal, (int)minPct, cfg->margin->maxVal, (int)maxPct);
	} else {
		dLine("  margin   :  N/A  (hw limits applied directly)");
	}

	dBot();
}


// =============================================================================
// 5. PUBLIC API
// =============================================================================

void dashboard_motion_register(const ComBus* bus, const EnvCfg* mach) {
	s_bus  = bus;
	s_mach = mach;
	dashboard_register_slot('6', "motion", render_motion_view);
	dashboard_register_detail('6', motion_detail_count, render_motion_detail);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_motion.cpp
