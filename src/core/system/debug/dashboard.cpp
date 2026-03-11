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
static uint8_t s_currentKey    = '1';

	// --- Full-clear flag ---
	// True on first render and on every view switch.  Subsequent periodic
	// refreshes use cursor-home-only (no erase) to avoid screen flicker.
static bool s_needFullClear = true;

	// --- Pre-built border lines (built once in dashboard_setup, sent as one write) ---
	// Each: left-corner (3) + 120×"─" (360) + right-corner (3) + \r\n (2) + NUL (1) = 369 bytes.
static char s_lineTop[369]; ///< ┌──…──┐
static char s_lineMid[369]; ///< ├──…──┤
static char s_lineBot[369]; ///< └──…──┘

	// --- Slot table ---
struct DashSlot {
	uint8_t      key;
	char         label[16];
	DashRenderFn fn;
};
static DashSlot s_slots[8];
static uint8_t  s_slotCount = 0;

	// --- Detail sub-view state ---
struct DashDetailSlot {
	uint8_t         slotKey;
	DashDetailCount countFn;
	DashDetailFn    renderFn;
};
static DashDetailSlot s_details[8];
static uint8_t        s_detailCount = 0;
static bool           s_inDetail    = false;
static uint8_t        s_detailIdx   = 0;

	// --- Event ring buffer ---
struct DashEvent {
	uint32_t ms;
	char     msg[56];
};
static DashEvent s_events[DashEventCount];
static uint8_t   s_eventHead  = 0;
static uint8_t   s_eventCount = 0;

	// --- Serial log tail ring buffer ---
static char    s_logLines[DashLogCount][DashInnerW + 1];
static uint8_t s_logHead  = 0;
static uint8_t s_logCount = 0;


// =============================================================================
// 2. FRAME PRIMITIVES
// =============================================================================

/** @brief Print ANSI clear-screen + cursor-home. */
void dClear() {
	Serial.print("\x1B[2J\x1B[H");
}

/** @brief Print a blank line before the frame (outside the border).
 *
 * @details Fills the line with spaces to overwrite any residual content
 *   from the previous render (cursor-home refresh leaves old chars in place).
 */
void dPre() {
	// (1 + DashInnerW + 1) spaces to overwrite old border, then CRLF
	char blank[128];
	memset(blank, ' ', 1 + DashInnerW + 1);
	blank[1 + DashInnerW + 1]   = '\r';
	blank[1 + DashInnerW + 2]   = '\n';
	Serial.write((const uint8_t*)blank, 1 + DashInnerW + 3);
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
 * @brief Print a mid-frame separator with an embedded text label.
 *
 * @details Renders: ├─── label ────...─┤\r\n
 *   Pads the right side with ─ characters to fill DashInnerW exactly.
 */
void dMidLabel(const char* label) {
	const uint8_t lc[3]   = {0xE2, 0x94, 0x9C};  // ├
	const uint8_t rc[3]   = {0xE2, 0x94, 0xA4};  // ┤
	const uint8_t dash[3] = {0xE2, 0x94, 0x80};  // ─
	const int prefixDashes = 3;
	int lblLen       = (int)strlen(label);
	int suffixDashes = (int)DashInnerW - prefixDashes - 1 - lblLen - 1;
	if (suffixDashes < 0) suffixDashes = 0;
		// ├ (3 B) + prefixDashes*3 + SP + lblLen + SP + suffixDashes*3 + ┤ (3 B) + CRLF
	char buf[512];
	int  pos = 0;
	buf[pos++] = (char)lc[0]; buf[pos++] = (char)lc[1]; buf[pos++] = (char)lc[2];
	for (int i = 0; i < prefixDashes; i++) {
		buf[pos++] = (char)dash[0]; buf[pos++] = (char)dash[1]; buf[pos++] = (char)dash[2];
	}
	buf[pos++] = ' ';
	memcpy(buf + pos, label, (size_t)lblLen); pos += lblLen;
	buf[pos++] = ' ';
	for (int i = 0; i < suffixDashes; i++) {
		buf[pos++] = (char)dash[0]; buf[pos++] = (char)dash[1]; buf[pos++] = (char)dash[2];
	}
	buf[pos++] = (char)rc[0]; buf[pos++] = (char)rc[1]; buf[pos++] = (char)rc[2];
	buf[pos++] = '\r'; buf[pos++] = '\n';
	Serial.write((const uint8_t*)buf, (size_t)pos);
}

/**
 * @brief Print a content line padded to DashInnerW: │<buf…spaces>│
 *
 * @details The entire line is assembled in a local buffer and sent in a
 *   single Serial.write() call — the cursor crosses the line in one step,
 *   eliminating the visible character artefact on content rows.
 */
void dContent(const char* buf) {
		// Layout: │(3) + content padded to DashInnerW(120) + │(3) + \r\n(2) = 128 bytes.
	char line[128];
		// Left border │ = U+2502 = E2 94 82
	line[0] = '\xE2'; line[1] = '\x94'; line[2] = '\x82';
		// Content — space-padded to exactly DashInnerW
	int len = (int)strlen(buf);
	if (len > (int)DashInnerW) len = (int)DashInnerW;
	memcpy(line + 3, buf, (size_t)len);
	memset(line + 3 + len, ' ', DashInnerW - (size_t)len);
		// Right border │ + CR LF
	line[123] = '\xE2'; line[124] = '\x94'; line[125] = '\x82';
	line[126] = '\r';   line[127] = '\n';
	Serial.write((const uint8_t*)line, 128);
}

void dContentAnsi(const char* buf, int visLen) {
		// Variable-length line: left border + content (with ANSI) + padding + right border + CRLF.
		// visLen = visible character count (ANSI bytes excluded) for correct padding.
	char line[DashInnerW + 80]; // extra headroom for ANSI sequences
	line[0] = '\xE2'; line[1] = '\x94'; line[2] = '\x82';
	int  len = (int)strlen(buf);
	int  pad = (int)DashInnerW - visLen;
	if (pad < 0) pad = 0;
	memcpy(line + 3, buf, (size_t)len);
	memset(line + 3 + len, ' ', (size_t)pad);
	int rOff = 3 + len + pad;
	line[rOff]   = '\xE2'; line[rOff+1] = '\x94'; line[rOff+2] = '\x82';
	line[rOff+3] = '\r';   line[rOff+4] = '\n';
	Serial.write((const uint8_t*)line, (size_t)(rOff + 5));
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
	uint32_t sec  = secs % 60;
	uint32_t min  = (secs / 60) % 60;
	uint32_t hr   = secs / 3600;
	snprintf(out, size, "%02u:%02u:%02u", hr, min, sec);
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

/** @brief Print the navigation bar below the frame — centered, single Serial.write(). */
static void dNav() {
		// Build content with uppercase labels and spaced colon
	char content[256];
	size_t cpos = 0;
	for (uint8_t i = 0; i < s_slotCount; i++) {
			// uppercase label copy
		char lbl[24];
		uint8_t l = 0;
		for (const char* p = s_slots[i].label; *p && l < sizeof(lbl)-1; p++)
			lbl[l++] = (char)toupper((unsigned char)*p);
		lbl[l] = '\0';
		cpos += (size_t)snprintf(content + cpos, sizeof(content) - cpos,
		                         "%c : %s    ", (char)s_slots[i].key, lbl);
	}
		// Show RETURN : DETAIL when the current slot has a registered detail with items
	for (uint8_t i = 0; i < s_detailCount; i++) {
		if (s_details[i].slotKey == s_currentKey) {
			uint8_t n = s_details[i].countFn ? s_details[i].countFn() : 0;
			if (n > 0)
				cpos += (size_t)snprintf(content + cpos, sizeof(content) - cpos, "RETURN : DETAIL    ");
			break;
		}
	}
	cpos += (size_t)snprintf(content + cpos, sizeof(content) - cpos, "Q : QUIT");
		// Center within visual frame width (1 + DashInnerW + 1)
	int pad = ((int)(1 + DashInnerW + 1) - (int)cpos) / 2;
	if (pad < 0) pad = 0;
	char   buf[300];
	size_t pos = 0;
	for (int i = 0; i < pad; i++) buf[pos++] = ' ';
	memcpy(buf + pos, content, cpos); pos += cpos;
	buf[pos++] = '\r'; buf[pos++] = '\n';
	Serial.write((const uint8_t*)buf, pos);
}

/** @brief Print the detail-mode navigation bar (replaces normal slot nav while in detail). */
static void dNavDetail() {
		// Find parent slot label for backtrack display
	const char* parentLabel = "back";
	for (uint8_t i = 0; i < s_slotCount; i++) {
		if (s_slots[i].key == s_currentKey) { parentLabel = s_slots[i].label; break; }
	}
		// Live sub-item count from countFn
	uint8_t n = 0;
	for (uint8_t i = 0; i < s_detailCount; i++) {
		if (s_details[i].slotKey == s_currentKey) {
			if (s_details[i].countFn) n = s_details[i].countFn();
			break;
		}
	}
		// Uppercase parent label
	char parentUpper[20];
	uint8_t l = 0;
	for (const char* p = parentLabel; *p && l < sizeof(parentUpper)-1; p++)
		parentUpper[l++] = (char)toupper((unsigned char)*p);
	parentUpper[l] = '\0';
		// Format: "- Ch X/N +  (- +)      PARENT detail      Q : back"
	char content[192];
	size_t cpos = (size_t)snprintf(content, sizeof(content),
	             "- Ch %u/%u +  (- +)      %s detail      Q : back",
	             s_detailIdx + 1, n, parentUpper);
		// Center within visual frame width (1 + DashInnerW + 1)
	int pad = ((int)(1 + DashInnerW + 1) - (int)cpos) / 2;
	if (pad < 0) pad = 0;
	char   buf[256];
	size_t pos = 0;
	for (int i = 0; i < pad; i++) buf[pos++] = ' ';
	memcpy(buf + pos, content, cpos); pos += cpos;
	buf[pos++] = '\r'; buf[pos++] = '\n';
	Serial.write((const uint8_t*)buf, pos);
}

/** @brief Dispatch redraw to the currently active slot (or detail sub-view). */
static void renderView() {
		// --- Position: full erase on first/view-switch, home+erase otherwise ---
	if (s_needFullClear) {
		Serial.print("\x1B[2J\x1B[H");  // erase full screen + cursor home
		s_needFullClear = false;
	} else {
		Serial.print("\x1B[H\x1B[J");   // cursor home + erase from cursor to end of screen
	}

		// --- Detail mode: dispatch to sub-view renderer ---
	if (s_inDetail) {
		for (uint8_t i = 0; i < s_detailCount; i++) {
			if (s_details[i].slotKey == s_currentKey) {
				if (s_details[i].renderFn) s_details[i].renderFn();
				dNavDetail();
				return;
			}
		}
		s_inDetail = false;  // detail slot no longer registered — drop back to normal
	}

		// --- Normal mode: dispatch to slot renderer ---
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
	s_currentKey   = '1';
	s_needFullClear = true;

		// --- 2. Clear slot table ---
	s_slotCount = 0;
	memset(s_slots, 0, sizeof(s_slots));

		// --- 2b. Clear detail sub-view table ---
	s_detailCount = 0;
	s_inDetail    = false;
	s_detailIdx   = 0;
	memset(s_details, 0, sizeof(s_details));

		// --- 3. Clear event ring buffer ---
	memset(s_events, 0, sizeof(s_events));
	s_eventHead  = 0;
	s_eventCount = 0;

		// --- 4. Clear log tail ring buffer ---
	memset(s_logLines, 0, sizeof(s_logLines));
	s_logHead  = 0;
	s_logCount = 0;

		// --- 5. Build pre-computed border lines ---
		// Each line: left-corner(3) + DashInnerW×"─"(3 bytes each) + right-corner(3) + \r\n(2) + NUL(1)
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
		const uint16_t rc_off = 3 + DashInnerW * 3;  // index of right-corner first byte
		p[rc_off]   = (char)rc[r][0];
		p[rc_off+1] = (char)rc[r][1];
		p[rc_off+2] = (char)rc[r][2];
		p[rc_off+3] = '\r'; p[rc_off+4] = '\n'; p[rc_off+5] = '\0';
	}

		// --- 6. Hide cursor for the entire dashboard session ---
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
 * @brief Register a detail sub-view for a slot.
 *
 * @details Silently ignores registration when the detail table is full.
 *
 * @param slotKey   Key of the parent slot.
 * @param countFn   Callback returning the number of sub-items (0 disables detail entry).
 * @param renderFn  Render callback; call dashboard_detail_index() inside to get current item.
 */
void dashboard_register_detail(uint8_t slotKey, DashDetailCount countFn, DashDetailFn renderFn) {
	if (s_detailCount >= 8) return;
	s_details[s_detailCount].slotKey  = slotKey;
	s_details[s_detailCount].countFn  = countFn;
	s_details[s_detailCount].renderFn = renderFn;
	s_detailCount++;
}

/**
 * @brief Return the 0-based index of the currently displayed detail sub-item.
 *
 * @details Only valid when called from within a DashDetailFn render callback.
 */
uint8_t dashboard_detail_index() {
	return s_detailIdx;
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
 * @brief Push a formatted serial log line into the log tail ring buffer.
 *
 * @details Called by log_impl (via debug.h) on every serial log output.
 *   Strips trailing \r\n.  Lines longer than DashInnerW are truncated.
 *
 * @param msg  Formatted log string.
 */
void dashboard_push_log(const char* msg) {
	strncpy(s_logLines[s_logHead], msg, DashInnerW);
	s_logLines[s_logHead][DashInnerW] = '\0';
		// Strip trailing CR/LF
	int len = (int)strlen(s_logLines[s_logHead]);
	while (len > 0 && (s_logLines[s_logHead][len-1] == '\n' || s_logLines[s_logHead][len-1] == '\r'))
		s_logLines[s_logHead][--len] = '\0';
	if (len == 0) return;  // skip blank lines
	s_logHead = (s_logHead + 1) % DashLogCount;
	if (s_logCount < DashLogCount) s_logCount++;
}

/**
 * @brief Render the serial log tail section.
 *
 * @details Shows the last DashLogCount lines received through log_impl,
 *   oldest at top, newest at bottom.  Must be called between dMid() and
 *   dBot() inside a frame.
 */
void dashboard_render_log() {
	if (s_logCount == 0) {
		dLine("  (no log output yet)");
	} else {
		uint8_t shown = (s_logCount < DashLogCount) ? s_logCount : DashLogCount;
		for (uint8_t k = 0; k < shown; k++) {
			uint8_t idx = (s_logHead + DashLogCount - shown + k) % DashLogCount;
			dLine("  %s", s_logLines[idx]);
		}
	}
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

			// ANSI escape sequence — arrow keys: \x1B [ C (right) or \x1B [ D (left)
		if (c == '\x1B') {
			uint32_t t0 = millis();
			while (!Serial.available() && (millis() - t0) < 10) {}
			if (Serial.available() && Serial.peek() == '[') {
				Serial.read();  // consume '['
				t0 = millis();
				while (!Serial.available() && (millis() - t0) < 10) {}
				if (Serial.available()) {
					char dir = (char)Serial.read();
					if (s_inDetail) {
						uint8_t n = 0;
						for (uint8_t i = 0; i < s_detailCount; i++) {
							if (s_details[i].slotKey == s_currentKey) {
								if (s_details[i].countFn) n = s_details[i].countFn();
								break;
							}
						}
						if (n > 0) {
							if      (dir == 'C') s_detailIdx = (uint8_t)((s_detailIdx + 1) % n);            // right → next
							else if (dir == 'D') s_detailIdx = s_detailIdx == 0 ? n - 1 : s_detailIdx - 1;  // left → prev
							s_lastRefresh   = 0;
							s_needFullClear = true;
						}
					}
				}
			}
			return;
		}

			// Enter: open detail sub-view for current slot (if registered and non-empty)
		if (c == '\r' || c == '\n') {
			for (uint8_t i = 0; i < s_detailCount; i++) {
				if (s_details[i].slotKey == s_currentKey) {
					uint8_t n = s_details[i].countFn ? s_details[i].countFn() : 0;
					if (n > 0) {
						s_inDetail      = true;
						s_detailIdx     = 0;
						s_lastRefresh   = 0;
						s_needFullClear = true;
					}
					break;
				}
			}
			return;
		}

			// Q/q: exit detail mode (back to parent), or suspend at top level
		if (c == 'Q' || c == 'q') {
			if (s_inDetail) {
				s_inDetail      = false;
				s_lastRefresh   = 0;
				s_needFullClear = true;
				return;
			}
			s_suspended = true;
			Serial.print("\x1B[?25h");  // restore cursor while suspended
			Serial.println("[DASH] Dashboard suspended \xe2\x80\x94 press any key to resume.");
			return;
		}

			// - + : detail navigation fallback (serial monitors that don't send ANSI arrows)
		if (s_inDetail && (c == '-' || c == '+')) {
			uint8_t n = 0;
			for (uint8_t i = 0; i < s_detailCount; i++) {
				if (s_details[i].slotKey == s_currentKey) {
					if (s_details[i].countFn) n = s_details[i].countFn();
					break;
				}
			}
			if (n > 0) {
				if      (c == '+') s_detailIdx = (uint8_t)((s_detailIdx + 1) % n);
				else if (c == '-') s_detailIdx = s_detailIdx == 0 ? n - 1 : s_detailIdx - 1;
				s_lastRefresh   = 0;
				s_needFullClear = true;
			}
			return;
		}

			// Slot selection keys (disabled while in detail mode)
		if (!s_inDetail) {
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
