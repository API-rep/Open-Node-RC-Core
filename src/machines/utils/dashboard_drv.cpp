/******************************************************************************
 * @file dashboard_drv.cpp
 * @brief ANSI terminal dashboard — Layer 3 DC-driver module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_drv.h"
#include <core/utils/debug/dashboard.h>

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

static const char* drvModeStr(DcDrvMode m) {
	switch (m) {
		case DcDrvMode::TWO_WAY_NEUTRAL_CENTER: return "BIPOLAR";
		case DcDrvMode::ONE_WAY:                return "ONE_WAY";
		case DcDrvMode::UNDEFINED:              return "---";
		default:                                return "---";
	}
}

static const char* devTypeStr(DcDevType t) {
	switch (t) {
		case DcDevType::DC_MOTOR:   return "DC motor";
		case DcDevType::DC_ACTUATOR: return "DC actuator";
		case DcDevType::SOLENOID:   return "Solenoid";
		case DcDevType::UNDEFINED:  return "---";
		default:                    return "---";
	}
}

static const char* devUsageStr(DevUsage u) {
	switch (u) {
		case DevUsage::GEN_WHEEL:    return "Wheel";
		case DevUsage::GEN_ACTUATOR: return "Actuator";
		case DevUsage::UNDEFINED:    return "---";
		default:                     return "---";
	}
}


// =============================================================================
// 3. VIEW RENDERER
// =============================================================================

/**
 * @brief Render the DC-driver view: live table (top) + config at init (bottom).
 */
static void render_drv_view() {
	if (!s_bus || !s_mach) return;

	char upt[12];
	fmtUptime(upt, sizeof(upt));

		// --- Live section ---
	dPre();
	dTop();
	{
		char modelStr[48]; modelStr[0] = '\0';
		for (uint8_t i = 0; i < s_mach->dcDevCount; i++) {
			const DcDevice& d = s_mach->dcDev[i];
			if (!d.drvPort || !d.drvPort->driverModel || !d.drvPort->driverModel->infoName) continue;
			const char* mdl = d.drvPort->driverModel->infoName;
			if (strstr(modelStr, mdl)) continue;
			if (modelStr[0]) snprintf(modelStr + strlen(modelStr), sizeof(modelStr) - strlen(modelStr), ", %s", mdl);
			else             snprintf(modelStr, sizeof(modelStr), "%s", mdl);
		}
		char left[64], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ DRIVERS ]  %s", modelStr[0] ? modelStr : "---");
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();
	{
		char fwStr[10], bkStr[10];
		if (s_mach->maxFwSpeed.has_value())   snprintf(fwStr, sizeof(fwStr), "%.0f%%", (double)*s_mach->maxFwSpeed);
		else snprintf(fwStr, sizeof(fwStr), "100%%");
		if (s_mach->maxBackSpeed.has_value()) snprintf(bkStr, sizeof(bkStr), "%.0f%%", (double)*s_mach->maxBackSpeed);
		else snprintf(bkStr, sizeof(bkStr), "100%%");
		dLine("  RunLevel: %-12s  DC devices: %u  Max FW: %-6s  Max BK: %s",
		      dashRunLevelStr(s_bus->runLevel), s_mach->dcDevCount, fwStr, bkStr);
	}
	dMid();
	dLine("  %-2s  %-32s  %-3s  %-6s  %-5s  %-7s  %s",
	      "ID", "Name", "Ch", "Raw", "Cmd%", "PolInv", "PWM");
	dMid();

	for (uint8_t i = 0; i < s_mach->dcDevCount; i++) {
		const DcDevice& d    = s_mach->dcDev[i];
		const char*     name = d.infoName ? d.infoName : "?";

		char pwmStr[12];
		if (d.pwmFreq) snprintf(pwmStr, sizeof(pwmStr), "%uHz", (unsigned)*d.pwmFreq);
		else           snprintf(pwmStr, sizeof(pwmStr), "---");

		if (d.comChannel.has_value()) {
			uint8_t  chIdx = static_cast<uint8_t>(d.comChannel.value());
			uint16_t raw   = s_bus->analogBus[chIdx].value;
			int16_t  pct   = (d.mode == DcDrvMode::ONE_WAY)
			                 ? dashPctOneway (raw, s_bus->analogBusMaxVal)
			                 : dashPctBipolar(raw, s_bus->analogBusMaxVal);
			dLine("  %2d  %-32.32s  %2u   %6u  %+4d%%  %-7s  %s",
			      d.ID, name, chIdx, raw, pct, d.polInv ? "INV" : "---", pwmStr);
		} else {
			dLine("  %2d  %-32.32s  --     --    --%   %-7s  %s  (clone)",
			      d.ID, name, d.polInv ? "INV" : "---", pwmStr);
		}
	}

	dBot();
}


// =============================================================================
// 4. DETAIL VIEW RENDERER
// =============================================================================

/**
 * @brief Return the number of DC devices (used as detail item count).
 */
static uint8_t drv_detail_count() {
	return s_mach ? s_mach->dcDevCount : 0;
}

/**
 * @brief Render the per-driver detail view: live state + config at init.
 */
static void render_drv_detail() {
	if (!s_bus || !s_mach) return;
	uint8_t          idx  = dashboard_detail_index();
	if (idx >= s_mach->dcDevCount) return;
	const DcDevice&  d    = s_mach->dcDev[idx];
	const char*      name = d.infoName ? d.infoName : "?";
	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[64], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ DRIVERS ]  #%d - %s detail", d.ID, name);
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();

		// ── LIVE STATE ── unified single row (clone and active devices same height)
	{
		char cmdStr[8], rawStr[8];
		if (d.comChannel.has_value()) {
			uint8_t  chIdx = static_cast<uint8_t>(d.comChannel.value());
			uint16_t raw   = s_bus->analogBus[chIdx].value;
			int16_t  pct   = (d.mode == DcDrvMode::ONE_WAY)
			                 ? dashPctOneway (raw, s_bus->analogBusMaxVal)
			                 : dashPctBipolar(raw, s_bus->analogBusMaxVal);
			snprintf(cmdStr, sizeof(cmdStr), "%+4d%%", pct);
			snprintf(rawStr, sizeof(rawStr), "%5u",   raw);
		} else {
			snprintf(cmdStr, sizeof(cmdStr), "  --%");
			snprintf(rawStr, sizeof(rawStr), "   --");
		}
		dLine("  Cmd :  %s  |  Raw :  %s  |  PolInv :  %s",
		      cmdStr, rawStr, d.polInv ? "INV" : "---");
	}
	dMid();

		// ── CONFIG AT INIT ──
	{
			// Channel + PWM on one line
		char chStr[32];
		if (d.comChannel.has_value()) {
			uint8_t ch = static_cast<uint8_t>(d.comChannel.value());
			if (d.pwmFreq) snprintf(chStr, sizeof(chStr), "ch%u @ %u Hz", ch, (unsigned)*d.pwmFreq);
			else           snprintf(chStr, sizeof(chStr), "ch%u", ch);
		} else {
			snprintf(chStr, sizeof(chStr), "---");
		}

			// Parent: look up name in device table
		char parentStr[48];
		if (d.parentID.has_value()) {
			uint8_t     pid   = d.parentID.value();
			const char* pname = "?";
			for (uint8_t j = 0; j < s_mach->dcDevCount; j++) {
				if (s_mach->dcDev[j].ID == (int8_t)pid) {
					pname = s_mach->dcDev[j].infoName ? s_mach->dcDev[j].infoName : "?";
					break;
				}
			}
			snprintf(parentStr, sizeof(parentStr), "#%d - %s", (int)pid, pname);
		} else {
			snprintf(parentStr, sizeof(parentStr), "---");
		}

			// Speed limits — merged on one line, fwStr/bkStr not needed

			// Port and driver model
		const char* portName  = (d.drvPort && d.drvPort->infoName) ? d.drvPort->infoName : "---";
		const char* modelName = (d.drvPort && d.drvPort->driverModel && d.drvPort->driverModel->infoName)
		                        ? d.drvPort->driverModel->infoName : "---";

		dLine("  Device type :  %s",  devTypeStr(d.DevType));
		dLine("  Usage :  %s",        devUsageStr(d.usage));
		dLine("  Port :  %s",         portName);
		dLine("  Driver model :  %s", modelName);
		dLine("  Mode :  %s",         drvModeStr(d.mode));
		dLine("  Channel :  %s",      chStr);
		dLine("  Parent :  %s",       parentStr);
		dLine("  Max forward/back speed :  %.1f / %.1f %%",
		      d.maxFwSpeed.value_or(100.0f), d.maxBackSpeed.value_or(100.0f));
	}

	dBot();
}


// =============================================================================
// 5. REGISTRATION
// =============================================================================

void dashboard_drv_register(const ComBus* bus, const Machine* mach) {
	s_bus  = bus;
	s_mach = mach;
	dashboard_register_slot('3', "drivers", render_drv_view);
	dashboard_register_detail('3', drv_detail_count, render_drv_detail);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_drv.cpp
