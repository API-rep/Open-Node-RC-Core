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

// --- Live section header ---
dTop();
dLine("  DRIVERS  live                               uptime: %s", upt);
dMid();
dLine("  RunLevel: %-12s  DC devices: %u",
dashRunLevelStr(s_bus->runLevel), s_mach->dcDevCount);
dMid();
dLine("  %-2s  %-24s  %-3s  %-5s  %-5s  %-6s",
"ID", "Name", "Ch", "Raw", "Cmd%", "PolInv");
dMid();

for (uint8_t i = 0; i < s_mach->dcDevCount; i++) {
const DcDevice& d = s_mach->dcDev[i];

if (d.comChannel.has_value()) {
uint8_t  chIdx = static_cast<uint8_t>(d.comChannel.value());
uint16_t raw   = s_bus->analogBus[chIdx].value;
int16_t  pct;
if (d.mode == DcDrvMode::ONE_WAY) {
pct = dashPctOneway(raw, s_bus->analogBusMaxVal);
} else {
pct = dashPctBipolar(raw, s_bus->analogBusMaxVal);
}
dLine("  %2d  %-24.24s  %2u   %5u  %+4d%%  %s",
d.ID, d.infoName,
chIdx, raw, pct,
d.polInv ? "INV" : "---"
);
} else {
dLine("  %2d  %-24.24s  --    --     -- %%  %s  (clone)",
d.ID, d.infoName,
d.polInv ? "INV" : "---"
);
}
}

// --- Config at init section ---
dMid();
dLine("  config (at init)");
dMid();
dLine("  %-2s  %-24s  %-7s  %-8s  %-6s  %-6s",
"ID", "Name", "Mode", "Ch", "FwMax", "BkMax");
dMid();

for (uint8_t i = 0; i < s_mach->dcDevCount; i++) {
const DcDevice& d = s_mach->dcDev[i];

char fwStr[8], bkStr[8], chStr[8];
if (d.maxFwSpeed.has_value())   snprintf(fwStr, sizeof(fwStr), "%5.1f%%", d.maxFwSpeed.value());
else                            snprintf(fwStr, sizeof(fwStr), " 100%%");
if (d.maxBackSpeed.has_value()) snprintf(bkStr, sizeof(bkStr), "%5.1f%%", d.maxBackSpeed.value());
else                            snprintf(bkStr, sizeof(bkStr), " 100%%");
if (d.comChannel.has_value())   snprintf(chStr, sizeof(chStr), "ch%u", static_cast<uint8_t>(d.comChannel.value()));
else                            snprintf(chStr, sizeof(chStr), "clone");

dLine("  %2d  %-24.24s  %-7s  %-8s  %s  %s",
d.ID, d.infoName,
drvModeStr(d.mode),
chStr, fwStr, bkStr
);
}

dBot();
}


// =============================================================================
// 4. REGISTRATION
// =============================================================================

void dashboard_drv_register(const ComBus* bus, const Machine* mach) {
s_bus  = bus;
s_mach = mach;
dashboard_register_slot('2', "drivers", render_drv_view);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_drv.cpp
