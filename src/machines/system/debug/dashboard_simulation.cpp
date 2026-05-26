/******************************************************************************
 * @file dashboard_simulation.cpp
 * @brief ANSI terminal dashboard — Layer 3 simulation module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_simulation.h"
#include <core/system/debug/dashboard.h>
#include <struct/simulation_struct.h>
#include <core/system/combus/processors/motion/cb_ramp.h>  // CbRampCfg, CbRampState
#include <core/system/combus/processors/base/cb_bypass.h>  // CbBypassCfg
#include <core/system/simulation/sim_gear.h>    // GearProcCfg, GearFsmState
#include <core/system/combus/combus_res.h>       // CbusNeutral (RPM computation)

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

/** @brief Short display name for an AnalogComBusID channel. */
static const char* aCh(AnalogComBusID id)
{
	switch (id) {
		case AnalogComBusID::RPM_BUS:              return "RPM";
		case AnalogComBusID::STEERING_BUS:         return "STEER_IN";
		case AnalogComBusID::DUMP_BUS:             return "DUMP_IN";
		case AnalogComBusID::ESC_SPEED_BUS:        return "ESC_SPD";
		case AnalogComBusID::GEAR:                 return "GEAR";
		case AnalogComBusID::DRIVE_STATE_BUS:      return "DRV_ST";
		case AnalogComBusID::BRAKE_BUS:            return "BRAKE";
		case AnalogComBusID::SUBGEAR_BUS:          return "SUBGEAR";
		case AnalogComBusID::DUMP_RAMPED_BUS:      return "DUMP_R";
		case AnalogComBusID::STEERING_RAMPED_BUS:  return "STEER_R";
		case AnalogComBusID::THROTTLE_BUS:         return "THROTTLE";
		default:                                    return "?";
	}
}

/** @brief Short display name for a DigitalComBusID channel. */
static const char* dCh(DigitalComBusID id)
{
	switch (id) {
		case DigitalComBusID::HORN_BTN:      return "HORN_BTN";
		case DigitalComBusID::LIGHTS:        return "LIGHTS";
		case DigitalComBusID::KEY_BTN:       return "KEY_BTN";
		case DigitalComBusID::BATTERY_LOW:   return "BAT_LOW";
		case DigitalComBusID::BRAKING:       return "BRAKING";
		case DigitalComBusID::DIRECT_DRIVE:     return "DIR_DRV";
		case DigitalComBusID::DIRECT_DRIVE_BTN: return "DIR_BTN";
		case DigitalComBusID::SUBGEAR_SET_BTN: return "SG_SET";
		case DigitalComBusID::SUBGEAR_UP_BTN:  return "SG_UP";
		case DigitalComBusID::SUBGEAR_DOWN_BTN:return "SG_DN";
		default:                             return "?";
	}
}

/// @brief Derive the primary input channel from a CbChain (ch.optInCh, analog).
static AnalogComBusID simChanInCh(const CbChain& ch)
{
	if (ch.optInCh.has_value() && std::holds_alternative<AnalogComBusID>(*ch.optInCh))
		return std::get<AnalogComBusID>(*ch.optInCh);
	return static_cast<AnalogComBusID>(0u);
}

/// @brief Derive the primary output channel from a CbChain (ch.outCh, analog).
static AnalogComBusID simChanOutCh(const CbChain& ch)
{
	if (ch.outCh.has_value() && std::holds_alternative<AnalogComBusID>(*ch.outCh))
		return std::get<AnalogComBusID>(*ch.outCh);
	return static_cast<AnalogComBusID>(0u);
}

/**
 * @brief Render one proc slot as a single dLine().
 *
 * @details Always produces exactly one dLine() call — guarantees uniform
 *   row height in the detail sub-view regardless of proc count.
 *   Shows config content for known proc types ("bypass", "ramp");
 *   generic fallback for others; "---" for empty slots (nullptr or fn==nullptr).
 *
 * @param idx   Slot index (0-based) shown in the row label.
 * @param proc  Pointer to the CbProc; nullptr or fn==nullptr → "---" row.
 */
static void render_proc_row(uint8_t idx, const CbProc* proc)
{
	if (proc == nullptr || proc->fn == nullptr) {
		dLine("  Proc %u : ---", (unsigned)idx);
		return;
	}
	const char* pname = proc->name ? proc->name : "?";

	if (strcmp(pname, "bypass") == 0) {
		DigitalComBusID condCh = DigitalComBusID{};
		if (proc->inCh[0].has_value() && std::holds_alternative<DigitalComBusID>(*proc->inCh[0]))
			condCh = std::get<DigitalComBusID>(*proc->inCh[0]);
		const bool active = s_bus->digitalBus[static_cast<uint8_t>(condCh)].value;
		dLine("  Proc %u : bypass    condCh: %-10s  active: %-3s",
		      (unsigned)idx, dCh(condCh), active ? "YES" : "no ");
	}
	else if (strcmp(pname, "ramp") == 0 && proc->cfg) {
		const CbRampCfg*   cfg = static_cast<const CbRampCfg*>(proc->cfg);
		const CbRampState* st  = static_cast<const CbRampState*>(proc->state);
		const uint16_t      pos = st ? st->currentPos : 0u;
		const int16_t       pct = st ? dashPctBipolar(pos, s_bus->analogBusMaxVal) : 0;
		if (cfg->accelDownSteps != 0u) {
			dLine("  Proc %u : ramp     period: %3u ms  accel+: %5u  accel-: %5u  brake: %5u  |  pos: %5u %+4d%%",
			      (unsigned)idx,
			      (unsigned)cfg->rampTimeMs,
			      (unsigned)cfg->accelSteps,
			      (unsigned)cfg->accelDownSteps,
			      (unsigned)cfg->brakeSteps,
			      (unsigned)pos, (int)pct);
		} else {
			dLine("  Proc %u : ramp     period: %3u ms  accel:  %5u  brake:  %5u  |  pos: %5u %+4d%%",
			      (unsigned)idx,
			      (unsigned)cfg->rampTimeMs,
			      (unsigned)cfg->accelSteps,
			      (unsigned)cfg->brakeSteps,
			      (unsigned)pos, (int)pct);
		}
	}
	else if (strcmp(pname, "gear-fsm") == 0 && proc->cfg) {
		const GearProcCfg*  cfg  = static_cast<const GearProcCfg*>(proc->cfg);
		const GearFsmState* st   = static_cast<const GearFsmState*>(proc->state);
		const int8_t        gear = st ? st->gear    : 0;
		const int8_t        sg   = st ? st->subGear : 0;
		const uint16_t      ramp = (gear >= 1)
		                         ? cfg->profile->gear[gear - 1].rampTime : 0u;
		//  RPM is now fed directly from RPM_BUS (seeded from inCh); read live from bus.
		const uint16_t rpm = s_bus->analogBus[static_cast<uint8_t>(AnalogComBusID::RPM_BUS)].value;
		const uint16_t maxRpm = cfg->profile->gear[cfg->profile->gearCount - 1].upShift;
		dLine("  Proc %u : gear-fsm  gear: %d/%d  sub: %d/%d  ramp: %4u ms  rpm: %4u/%u",
		      (unsigned)idx,
		      (int)gear, (int)cfg->profile->gearCount,
		      (int)sg,   (int)cfg->profile->subGearCount,
		      (unsigned)ramp,
		      (unsigned)rpm,  (unsigned)maxRpm);
	}
	else if (strcmp(pname, "ratio") == 0 && proc->cfg) {
		const GearProcCfg* cfg  = static_cast<const GearProcCfg*>(proc->cfg);
		const uint16_t     curG = s_bus->analogBus[static_cast<uint8_t>(AnalogComBusID::GEAR)].value;
		{
			char rbuf[48]; int rp = 0;
			for (uint8_t g = 0u; g < cfg->profile->gearCount && rp < 40; ++g)
				rp += snprintf(rbuf + rp, sizeof(rbuf) - (size_t)rp,
				               "G%u:Δ%-4d  ", g + 1u, (int)cfg->profile->gear[g].shiftDelta);
			dLine("  Proc %u : shift-Δ  G%u active  |  %s",
			      (unsigned)idx, (unsigned)curG, rbuf);
		}
	}
	else {
		dLine("  Proc %u : %-10s  fn: %s",
		      (unsigned)idx, pname, proc->fn ? "active" : "null");
	}
}


// =============================================================================
// 3. MAIN VIEW  (CbChain pipeline table)
// =============================================================================

static void render_sim_view()
{
	if (!s_bus || !s_mach) return;

	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[72], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ SIMULATION ]  pipeline — %u channel%s",
		                    (unsigned)s_mach->simChainCount,
		                    s_mach->simChainCount != 1u ? "s" : "");
		lLen -= 2;  // '—' (U+2014) = 3 UTF-8 bytes, 1 display char → compensate 2 extra bytes
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();
	dLine("  %-2s  %-12s  %-10s  %-10s  %7s  %7s  %-7s  procs",
	      "#", "name", "inCh", "outCh", "in %", "out %", "bypass");
	dMid();
	for (uint8_t i = 0; i < s_mach->simChainCount; ++i) {
		const CbChain&  ch     = s_mach->simChain[i];
		const AnalogComBusID chIn  = simChanInCh(ch);
		const AnalogComBusID chOut = simChanOutCh(ch);
		const uint16_t    inVal  = s_bus->analogBus[static_cast<uint8_t>(chIn)].value;
		const uint16_t    outVal = s_bus->analogBus[static_cast<uint8_t>(chOut)].value;
		const int16_t     inPct  = dashPctBipolar(inVal,  s_bus->analogBusMaxVal);
		const int16_t     outPct = dashPctBipolar(outVal, s_bus->analogBusMaxVal);

		//  out display: GEAR channel shows raw gear integer instead of %.
		char outDisp[10];
		if (chOut == AnalogComBusID::GEAR) {
			snprintf(outDisp, sizeof(outDisp), "  G:%u    ", (unsigned)outVal);
		} else {
			snprintf(outDisp, sizeof(outDisp), "%+6d%%", (int)outPct);
		}

		//  Bypass state: first proc named "bypass" with inCh[0] HIGH.
		bool chBypass = false;
		if (ch.procCount > 0u && ch.procs != nullptr
		    && ch.procs[0].name != nullptr
		    && strcmp(ch.procs[0].name, "bypass") == 0)
		{
			const auto& cond = ch.procs[0].inCh[0];
			if (cond.has_value() && std::holds_alternative<DigitalComBusID>(*cond))
				chBypass = s_bus->digitalBus[static_cast<uint8_t>(std::get<DigitalComBusID>(*cond))].value;
		}

		//  Proc names — '+'-separated, up to 4 entries, truncated at 26 chars.
		char procNames[28] = "";
		if (ch.procCount == 0u || ch.procs == nullptr) {
			snprintf(procNames, sizeof(procNames), "(none)");
		} else {
			for (uint8_t p = 0u; p < ch.procCount && p < 4u; ++p) {
				if (p > 0u) strncat(procNames, "+", sizeof(procNames) - strlen(procNames) - 1u);
				const char* pn = ch.procs[p].name ? ch.procs[p].name : "?";
				strncat(procNames, pn, sizeof(procNames) - strlen(procNames) - 1u);
			}
		}

		dLine("  %2u  %-12s  %-10s  %-10s  %+6d%%  %7s  %-7s  %s",
		      (unsigned)i,
		      ch.name ? ch.name : "?",
		      aCh(chIn), aCh(chOut),
		      (int)inPct, outDisp,
		      chBypass ? "BYPASS" : "---",
		      procNames);
	}
	dBot();
}


// =============================================================================
// 4. DETAIL VIEW  (one sub-item per CbChain)
// =============================================================================

/// Number of proc rows always rendered per detail sub-view (uniform height).
static constexpr uint8_t kMaxProcRows = 4u;

static void render_channel_detail(uint8_t idx)
{
	if (!s_bus || !s_mach || idx >= s_mach->simChainCount) return;

	const CbChain& ch     = s_mach->simChain[idx];
	const AnalogComBusID chIn  = simChanInCh(ch);
	const AnalogComBusID chOut = simChanOutCh(ch);
	const uint16_t    inVal  = s_bus->analogBus[static_cast<uint8_t>(chIn)].value;
	const uint16_t    outVal = s_bus->analogBus[static_cast<uint8_t>(chOut)].value;
	const int16_t     inPct  = dashPctBipolar(inVal,  s_bus->analogBusMaxVal);
	const int16_t     outPct = dashPctBipolar(outVal, s_bus->analogBusMaxVal);

	//  Bypass state for this channel.
	bool chBypass = false;
	if (ch.procCount > 0u && ch.procs != nullptr
	    && ch.procs[0].name != nullptr
	    && strcmp(ch.procs[0].name, "bypass") == 0)
	{
		const auto& cond = ch.procs[0].inCh[0];
		if (cond.has_value() && std::holds_alternative<DigitalComBusID>(*cond))
			chBypass = s_bus->digitalBus[static_cast<uint8_t>(std::get<DigitalComBusID>(*cond))].value;
	}

	char upt[12];
	fmtUptime(upt, sizeof(upt));

	dPre();
	dTop();
	{
		char left[80], right[28];
		int lLen = snprintf(left,  sizeof(left),  "  [ SIMULATION ]  #%u - %s  (%s \u2192 %s)",
		                    (unsigned)(idx + 1u),
		                    ch.name ? ch.name : "?",
		                    aCh(chIn), aCh(chOut));
		lLen -= 2;  // '→' (U+2192) = 3 UTF-8 bytes, 1 display char → compensate 2 extra bytes
		int rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
		dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
	}
	dMid();
	//  Live row.
	dLine("  in: %5u %+4d%%  |  out: %5u %+4d%%  |  bypass: %-3s   (live)",
	      (unsigned)inVal, (int)inPct,
	      (unsigned)outVal, (int)outPct,
	      chBypass ? "ON " : "OFF");
	dMidLabel("config");
	//  Proc rows — always kMaxProcRows lines regardless of proc count (uniform height).
	for (uint8_t p = 0u; p < kMaxProcRows; ++p) {
		const CbProc* proc = (ch.procs != nullptr && p < ch.procCount)
		                      ? &ch.procs[p]
		                      : nullptr;
		render_proc_row(p, proc);
	}
	dBot();
}

static uint8_t sim_detail_count()
{
	return s_mach ? s_mach->simChainCount : 0u;
}

static void render_sim_detail()
{
	if (!s_bus || !s_mach) return;
	render_channel_detail(dashboard_detail_index());
}


// =============================================================================
// 5. PUBLIC API
// =============================================================================

void dashboard_simulation_register(const ComBus* bus, const EnvCfg* mach) {
	s_bus  = bus;
	s_mach = mach;
	dashboard_register_slot('6', "simulation", render_sim_view);
	dashboard_register_detail('6', sim_detail_count, render_sim_detail);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_simulation.cpp
