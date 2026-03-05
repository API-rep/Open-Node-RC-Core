/******************************************************************************
 * @file dashboard_input.cpp
 * @brief ANSI terminal dashboard — Layer 3 inputs/combus module view.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_input.h"
#include <core/utils/debug/dashboard.h>

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
 * @brief Render the inputs view: analog table (top) + combus state (bottom).
 */
static void render_input_view() {
if (!s_bus) return;

char upt[12];
fmtUptime(upt, sizeof(upt));

// --- Live section header ---
dTop();
dLine("  INPUTS  live                                uptime: %s", upt);
dMid();

// --- Analog channels ---
dLine("  %-3s  %-26s  %-6s  %-5s  %s",
"CH", "Name", "Raw", "Pct", "Drived");
dMid();
for (uint8_t i = 0; i < s_analogCh; i++) {
uint16_t raw = s_bus->analogBus[i].value;
int16_t  pct = dashPctBipolar(raw, s_bus->analogBusMaxVal);
dLine("  %2u   %-26.26s  %5u  %+4d%%  %s",
i,
s_bus->analogBus[i].infoName,
raw, pct,
s_bus->analogBus[i].isDrived ? "yes" : "no"
);
}

// --- Digital channels ---
dMid();
dLine("  %-3s  %-26s  %-5s  %s",
"CH", "Name", "Val", "Drived");
dMid();
for (uint8_t i = 0; i < s_digitalCh; i++) {
dLine("  %2u   %-26.26s  %-5s  %s",
i,
s_bus->digitalBus[i].infoName,
s_bus->digitalBus[i].value ? "ON" : "OFF",
s_bus->digitalBus[i].isDrived ? "yes" : "no"
);
}

// --- ComBus runtime state (config at init) ---
dMid();
dLine("  combus state");
dMid();
dLine("  RunLevel: %-14s  keyOn: %-3s  battLow: %s",
dashRunLevelStr(s_bus->runLevel),
s_bus->keyOn        ? "YES" : "NO",
s_bus->batteryIsLow ? "YES" : "NO"
);
dLine("  analogBusMaxVal: %lu   analogCh: %u   digitalCh: %u",
s_bus->analogBusMaxVal, s_analogCh, s_digitalCh);

dBot();
}


// =============================================================================
// 3. REGISTRATION
// =============================================================================

void dashboard_input_register(const ComBus* bus, uint8_t analogCh, uint8_t digitalCh) {
s_bus       = bus;
s_analogCh  = analogCh;
s_digitalCh = digitalCh;
dashboard_register_slot('1', "inputs", render_input_view);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_input.cpp
