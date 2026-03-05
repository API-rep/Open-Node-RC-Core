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

dTop();
dLine("  BATTERY  live                               uptime: %s", upt);
dMid();

uint8_t count = vbat_channel_count();
if (count == 0) {
dEmpty();
dLine("  VBAT_SENSING not active — no channels configured.");
dEmpty();
} else {
dLine("  %-3s  %-8s  %-5s  %s", "Ch", "Voltage", "Cells", "Status");
dMid();
for (uint8_t i = 0; i < count; i++) {
float   v    = vbat_voltage(i);
uint8_t cells = vbat_cells(i);
bool    low  = vbat_is_low(i);
char    cellStr[8];
if (cells > 0) snprintf(cellStr, sizeof(cellStr), "%dS", cells);
else           snprintf(cellStr, sizeof(cellStr), "---");
dLine("  %2u   %5.2f V    %-5s  %s",
i, v, cellStr, low ? "LOW" : "OK"
);
}
}

dBot();
}


// =============================================================================
// 2. REGISTRATION
// =============================================================================

void dashboard_vbat_register() {
dashboard_register_slot('3', "battery", render_vbat_view);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_vbat.cpp
