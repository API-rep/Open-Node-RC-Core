/******************************************************************************
 * @file dashboard.cpp
 * @brief ANSI terminal dashboard — Layer 1 core shell implementation.
 *
 * @details Frame primitives, slot table, keyboard dispatch, event ring
 *   buffer, and refresh timer.  All machine/module knowledge lives in
 *   higher layers (dashboard_machine, dashboard_drv, etc.).
 *
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


// =============================================================================
// 1. PRIVATE STATE
// =============================================================================

	// --- Timing ---
static uint32_t s_startMs     = 0;
static uint32_t s_lastRefresh = 0;
static bool     s_suspended   = false;

	// --- Current active slot key ---
static uint8_t s_currentKey    = '0';

	// --- Full-clear flag ---
	// True on first render and on every view switch.  Subsequent periodic
	// refreshes use cursor-home-only (no erase) to avoid screen flicker.
static bool s_needFullClear = true;

	// --- Pre-built border lines (built once in dashboard_setup, sent as one write) ---
	// Each: left-corner (3) + 70×"─" (210) + right-corner (3) + \r\n (2) + NUL (1) = 219 bytes.
static char s_lineTop[219]; ///< ┌──…──┐
static char s_lineMid[219]; ///< ├──…──┤
static char s_lineBot[219]; ///< └──…──┘

	// --- Slot table ---
struct DashSlot {
	uint8_t      key;
	char         label[16];
	DashRenderFn fn;
};
static DashSlot s_slots[8];
static uint8_t  s_slotCount = 0;

	// --- Event ring buffer ---
struct DashEvent {
	uint32_t ms;
	char     msg[56];
};
static DashEvent s_events[DashEventCount];
static uint8_t   s_eventHead  = 0;
static uint8_t   s_eventCount = 0;


// =============================================================================
// 2. FRAME PRIMITIVES
// =============================================================================

/** @brief Print ANSI clear-screen + cursor-home. */
void dClear() {
	Serial.print("\x1B[2J\x1B[H");
}

/** @brief Print the top border of the frame. */
void dTop() {
	Serial.print(s_lineTop);
}

/** @brief Print the bottom border of the frame. */
void dBot() {
	Serial.print(s_lineBot);
}

/** @brief Print a mid-frame separator. */
void dMid() {
	Serial.print(s_lineMid);
}

/**
 * @brief Print a content line padded to DashInnerW: │<buf…spaces>│
 *
 * @details The entire line is assembled in a local buffer and sent in a
 *   single Serial.write() call — the cursor crosses the line in one step,
 *   eliminating the visible character artefact on content rows.
 */
void dContent(const char* buf) {
		// Layout: │(3) + content padded to DashInnerW(70) + │(3) + \r\n(2) = 78 bytes.
	char line[78];
		// Left border │ = U+2502 = E2 94 82
	line[0] = '\xE2'; line[1] = '\x94'; line[2] = '\x82';
		// Content — space-padded to exactly DashInnerW
	int len = (int)strlen(buf);
	if (len > (int)DashInnerW) len = (int)DashInnerW;
	memcpy(line + 3, buf, (size_t)len);
	memset(line + 3 + len, ' ', DashInnerW - (size_t)len);
		// Right border │ + CR LF
	line[73] = '\xE2'; line[74] = '\x94'; line[75] = '\x82';
	line[76] = '\r';   line[77] = '\n';
	Serial.write((const uint8_t*)line, 78);
}

/**
 * @brief Print a printf-formatted content line (truncated at DashInnerW).
 *
 * @param fmt  printf-style format string.
 */
void dLine(const char* fmt, ...) {
	char buf[DashInnerW + 1];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	dContent(buf);
}

/** @brief Print an empty content line: \u2502<spaces>\u2502 */
void dEmpty() {
	dContent("");
}


// =============================================================================
// 3. UTILITY HELPERS
// =============================================================================

/** @brief Current uptime in milliseconds since dashboard_setup(). */
uint32_t dashUptimeMs() {
	return millis() - s_startMs;
}

/**
 * @brief Format uptime as "MM:SS.t" into caller-supplied buffer.
 *
 * @param out   Destination buffer (minimum 10 bytes).
 * @param size  Buffer size.
 */
void fmtUptime(char* out, size_t size) {
	uint32_t ms   = dashUptimeMs();
	uint32_t secs = ms / 1000;
	uint32_t frac = (ms % 1000) / 100;
	uint32_t sec  = secs % 60;
	uint32_t min  = secs / 60;
	snprintf(out, size, "%02u:%02u.%u", min, sec, frac);
}

/**
 * @brief Format relative event age as "-MM:SS.t" into caller-supplied buffer.
 *
 * @param eventMs  Absolute uptime value at push time.
 * @param out      Destination buffer (minimum 11 bytes).
 * @param size     Buffer size.
 */
void fmtEventTime(uint32_t eventMs, char* out, size_t size) {
	uint32_t dt   = dashUptimeMs() - eventMs;
	uint32_t secs = dt / 1000;
	uint32_t frac = (dt % 1000) / 100;
	uint32_t sec  = secs % 60;
	uint32_t min  = secs / 60;
	snprintf(out, size, "-%02u:%02u.%u", min, sec, frac);
}

/**
 * @brief Render the event ring buffer section.
 *
 * @details Prints up to DashEventCount lines (or a placeholder when empty).
 *   Must be called after a dMid() and before dBot() inside a frame.
 */
void dashboard_render_events() {
	if (s_eventCount == 0) {
		dLine("  (no events yet)");
	} else {
		uint8_t shown = (s_eventCount < DashEventCount) ? s_eventCount : DashEventCount;
		for (uint8_t k = 0; k < shown; k++) {
			uint8_t idx = (s_eventHead + DashEventCount - shown + k) % DashEventCount;
			char    ts[12];
			fmtEventTime(s_events[idx].ms, ts, sizeof(ts));
			dLine("  [%s]  %.52s", ts, s_events[idx].msg);
		}
	}
}

/**
 * @brief Map a RunLevel to a human-readable string.
 *
 * @param rl  RunLevel value.
 * @return Pointer to a static literal string.
 */
const char* dashRunLevelStr(RunLevel rl) {
	switch (rl) {
		case RunLevel::IDLE:        return "IDLE";
		case RunLevel::STARTING:    return "STARTING";
		case RunLevel::RUNNING:     return "RUNNING";
		case RunLevel::TURNING_OFF: return "TURNING_OFF";
		case RunLevel::SLEEPING:    return "SLEEPING";
		case RunLevel::RESET:       return "RESET";
		default:                    return "---";
	}
}


// =============================================================================
// 4. PRIVATE HELPERS
// =============================================================================

/** @brief Print the navigation bar below the frame from registered slots. */
static void dNav() {
	Serial.print("  ");
	for (uint8_t i = 0; i < s_slotCount; i++) {
		Serial.print((char)s_slots[i].key);
		Serial.print(':');
		Serial.print(s_slots[i].label);
		if (i < s_slotCount - 1) Serial.print("  ");
	}
	Serial.println("  Q:quit");
}

/** @brief Dispatch redraw to the currently active slot. */
static void renderView() {
		// --- Position: full erase on first/view-switch, home-only otherwise ---
	if (s_needFullClear) {
		Serial.print("\x1B[2J\x1B[H");  // erase screen + cursor home
		s_needFullClear = false;
	} else {
		Serial.print("\x1B[H");          // cursor home only — overwrite in place
	}

	for (uint8_t i = 0; i < s_slotCount; i++) {
		if (s_slots[i].key == s_currentKey) {
			if (s_slots[i].fn) s_slots[i].fn();
			dNav();
			return;
		}
	}
		// Fallback: if active key no longer exists, go to first slot.
	if (s_slotCount > 0 && s_slots[0].fn) {
		s_currentKey = s_slots[0].key;
		s_slots[0].fn();
		dNav();
	}
}


// =============================================================================
// 5. PUBLIC API
// =============================================================================

/**
 * @brief Initialize core state — record start timestamp, reset all state.
 *
 * @details Called by the env-layer setup (dashboard_machine_setup).
 */
void dashboard_setup() {
		// --- 1. Record start time ---
	s_startMs      = millis();
	s_lastRefresh  = 0;
	s_suspended    = false;
	s_currentKey   = '0';
	s_needFullClear = true;

		// --- 2. Clear slot table ---
	s_slotCount = 0;
	memset(s_slots, 0, sizeof(s_slots));

		// --- 3. Clear event ring buffer ---
	memset(s_events, 0, sizeof(s_events));
	s_eventHead  = 0;
	s_eventCount = 0;

		// --- 4. Build pre-computed border lines ---
		// Each line: left-corner(3) + 70×"─"(210) + right-corner(3) + \r\n(2) + NUL(1) = 219 bytes.
		// "─" = U+2500 = E2 94 80
		// Corners:  ┌ E2 94 8C │ ┐ E2 94 90  (top)
		//           ├ E2 94 9C │ ┤ E2 94 A4  (mid)
		//           └ E2 94 94 │ ┘ E2 94 98  (bot)
	const uint8_t lc[3][3] = {{0xE2,0x94,0x8C},{0xE2,0x94,0x9C},{0xE2,0x94,0x94}};
	const uint8_t rc[3][3] = {{0xE2,0x94,0x90},{0xE2,0x94,0xA4},{0xE2,0x94,0x98}};
	char* lines[3]          = { s_lineTop, s_lineMid, s_lineBot };
	for (uint8_t r = 0; r < 3; r++) {
		char* p = lines[r];
		p[0] = (char)lc[r][0]; p[1] = (char)lc[r][1]; p[2] = (char)lc[r][2];
		for (uint8_t i = 0; i < DashInnerW; i++) {
			p[3 + i*3]   = '\xE2';
			p[3 + i*3+1] = '\x94';
			p[3 + i*3+2] = '\x80';
		}
		p[213] = (char)rc[r][0]; p[214] = (char)rc[r][1]; p[215] = (char)rc[r][2];
		p[216] = '\r'; p[217] = '\n'; p[218] = '\0';
	}

		// --- 5. Hide cursor for the entire dashboard session ---
		// Restored to visible only when the dashboard is suspended (Q key).
	Serial.print("\x1B[?25l");
}

/**
 * @brief Register a render slot accessible by keyboard key.
 *
 * @details Silently ignores registration when the slot table is full.
 *
 * @param key    Keyboard key character selecting this view.
 * @param label  Short nav-bar label (max 15 chars, NUL-terminated).
 * @param fn     Render callback.
 */
void dashboard_register_slot(uint8_t key, const char* label, DashRenderFn fn) {
	if (s_slotCount >= 8) return;
	s_slots[s_slotCount].key = key;
	strncpy(s_slots[s_slotCount].label, label, sizeof(s_slots[0].label) - 1);
	s_slots[s_slotCount].label[sizeof(s_slots[0].label) - 1] = '\0';
	s_slots[s_slotCount].fn  = fn;
	s_slotCount++;
}

/**
 * @brief Push a timestamped event message to the ring buffer.
 *
 * @details Overwrites the oldest entry when the buffer is full.
 *   Messages longer than 55 characters are silently truncated.
 *
 * @param msg  Short event description string.
 */
void dashboard_push_event(const char* msg) {
	DashEvent& e = s_events[s_eventHead];
	e.ms = dashUptimeMs();
	strncpy(e.msg, msg, sizeof(e.msg) - 1);
	e.msg[sizeof(e.msg) - 1] = '\0';
	s_eventHead = (s_eventHead + 1) % DashEventCount;
	if (s_eventCount < DashEventCount) s_eventCount++;
}

/**
 * @brief Update the dashboard: handle keyboard input, redraw on interval.
 *
 * @details Non-blocking. Designed to be called once per loop() iteration.
 */
void dashboard_update() {
		// --- 1. Suspended state — wait for any key to resume ---
	if (s_suspended) {
		if (Serial.available()) {
			Serial.read();
			s_suspended     = false;
			s_lastRefresh   = 0;
			s_needFullClear = true;  // redraw from scratch after suspend
			Serial.print("\x1B[?25l");  // hide cursor again for dashboard
		}
		return;
	}

		// --- 2. Keyboard input ---
	if (Serial.available()) {
		char c = (char)Serial.read();
		if (c == 'Q' || c == 'q') {
			s_suspended = true;
			Serial.print("\x1B[?25h");  // restore cursor while suspended
			Serial.println("[DASH] Dashboard suspended \u2014 press any key to resume.");
			return;
		}
			// Look up the key in the slot table.
		for (uint8_t i = 0; i < s_slotCount; i++) {
			if (s_slots[i].key == (uint8_t)c ||
			    (c >= 'a' && c <= 'z' && s_slots[i].key == (uint8_t)(c - 32))) {
				s_currentKey    = s_slots[i].key;
				s_lastRefresh   = 0;      // force immediate redraw
				s_needFullClear = true;   // clear artefacts from previous view
				break;
			}
		}
	}

		// --- 3. Timed redraw ---
	uint32_t now = millis();
	if ((now - s_lastRefresh) >= DashRefreshMs) {
		s_lastRefresh = now;
		renderView();
	}
}


#endif // DEBUG_DASHBOARD

// EOF dashboard.cpp
