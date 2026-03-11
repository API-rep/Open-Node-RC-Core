/******************************************************************************
 * @file dashboard.h
 * @brief ANSI terminal dashboard — Layer 1 core shell.
 *
 * @details Pure rendering primitives, slot registration, keyboard dispatch,
 *   event ring buffer, and refresh timer.  Zero knowledge of any machine
 *   type, view, or module.
 *
 *   Module views (Layer 3) call the frame primitives and utility helpers
 *   declared here to compose their own screens.
 *
 *   All public functions degrade to inline no-ops when DEBUG_DASHBOARD is
 *   not defined — callers compile cleanly in release builds without any
 *   #ifdef guards.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. GUARDED SECTION — compiles only with -D DEBUG_DASHBOARD
// =============================================================================

#ifdef DEBUG_DASHBOARD

#include <stdint.h>
#include <stddef.h>

#include <defs/machines_defs.h>


// =============================================================================
// 2. CONSTANTS
// =============================================================================

constexpr uint8_t  DashEventCount = 6;    ///< Event ring buffer depth.
constexpr uint8_t  DashLogCount   = 6;    ///< Serial log tail depth.
constexpr uint32_t DashRefreshMs  = 200;  ///< Redraw interval in ms.
constexpr uint8_t  DashInnerW     = 120;  ///< Inner content width in characters.


// =============================================================================
// 3. TYPES
// =============================================================================

/** @brief Render callback registered per slot. */
using DashRenderFn = void (*)(void);

/**
 * @brief Render callback for a detail sub-view item.
 *
 * @details Called by core when detail mode is active for the owning slot.
 *   Use dashboard_detail_index() to determine which sub-item to render.
 */
using DashDetailFn = void (*)(void);

/**
 * @brief Returns the number of available detail sub-items for a slot (queried at runtime).
 *
 * @details Returning 0 disables detail mode entry for that slot.
 */
using DashDetailCount = uint8_t (*)(void);


// =============================================================================
// 4. FRAME PRIMITIVES  (used by module-view files)
// =============================================================================

/** @brief Print ANSI clear-screen + cursor-home. */
void dClear();

/** @brief Print a blank line before the frame (outside the border). */
void dPre();

/** @brief Print the top border of the frame. */
void dTop();

/** @brief Print a mid-frame separator. */
void dMid();
/**
 * @brief Print a mid-frame separator with an embedded label.
 *
 *   ├─── label ────────────────────────────────────────┤
 *
 * @param label  Short section title embedded into the separator.
 */
void dMidLabel(const char* label);
/** @brief Print the bottom border of the frame. */
void dBot();

/**
 * @brief Print a plain content line padded to DashInnerW.
 *
 * @param buf  NUL-terminated string; characters beyond DashInnerW are clipped.
 */
void dContent(const char* buf);

/**
 * @brief Print a content line containing ANSI escape sequences.
 *
 *  Same as dContent() but uses @p visLen (visible characters, excluding invisible
 *  ANSI bytes) for the right-border padding calculation, preventing misalignment.
 *
 * @param buf     NUL-terminated string, may include ANSI escape codes.
 * @param visLen  Number of visible (non-escape) characters in @p buf.
 */
void dContentAnsi(const char* buf, int visLen);

/**
 * @brief Print a printf-formatted content line (truncated at DashInnerW).
 *
 * @param fmt  printf-style format string.
 */
void dLine(const char* fmt, ...);

/** @brief Print an empty content line. */
void dEmpty();


// =============================================================================
// 5. UTILITY HELPERS  (used by module-view files)
// =============================================================================

/** @brief Current uptime in milliseconds since dashboard_setup(). */
uint32_t dashUptimeMs();

/**
 * @brief Format uptime as "MM:SS" into caller-supplied buffer.
 *
 * @param out   Destination buffer (minimum 8 bytes).
 * @param size  Buffer size.
 */
void fmtUptime(char* out, size_t size);

/**
 * @brief Format relative event age as "-MM:SS.t" into caller-supplied buffer.
 *
 * @param eventMs  Absolute timestamp returned by dashUptimeMs() at push time.
 * @param out      Destination buffer (minimum 11 bytes).
 * @param size     Buffer size.
 */
void fmtEventTime(uint32_t eventMs, char* out, size_t size);

/**
 * @brief Render the event ring buffer section into the current frame.
 *
 * @details Prints up to DashEventCount lines (or a "(no events yet)"
 *   placeholder).  Must be called between dMid() and dBot() by the
 *   overview view.
 */
void dashboard_render_events();

/**
 * @brief Push a line of serial log output into the log tail ring buffer.
 *
 * @details Called automatically by log_impl when DEBUG_DASHBOARD is active.
 *   Lines longer than 70 characters are silently truncated.
 *
 * @param msg  Formatted log string (\n stripped).
 */
void dashboard_push_log(const char* msg);

/**
 * @brief Render the serial log tail section into the current frame.
 *
 * @details Prints up to DashLogCount lines (or a placeholder when empty).
 *   Call between dMid() and dBot().
 */
void dashboard_render_log();

/**
 * @brief Map a RunLevel to a human-readable string.
 *
 * @param rl  RunLevel value from core_defs.h.
 * @return Pointer to a static literal string.
 */
const char* dashRunLevelStr(RunLevel rl);

/**
 * @brief Map a raw combus value to a bipolar percent [-100, +100].
 *
 * @param raw     Raw analog bus value.
 * @param maxVal  Maximum bus value (typically 65535).
 */
inline int16_t dashPctBipolar(uint16_t raw, uint32_t maxVal) {
return (int16_t)(((int32_t)raw - (int32_t)(maxVal / 2)) * 200 / (int32_t)maxVal);
}

/**
 * @brief Map a raw combus value to a one-way percent [0, 100].
 *
 * @param raw     Raw analog bus value.
 * @param maxVal  Maximum bus value (typically 65535).
 */
inline int16_t dashPctOneway(uint16_t raw, uint32_t maxVal) {
return (int16_t)((int32_t)raw * 100 / (int32_t)maxVal);
}


// =============================================================================
// 6. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the core dashboard shell — record start time, reset state.
 *
 * @details Called internally by the env-layer setup (dashboard_machine_setup).
 *   Not intended to be called directly from application code.
 */
void dashboard_setup();

/**
 * @brief Register a render slot activated by a keyboard key.
 *
 * @details Slot '0' is reserved for the env-layer overview and must be
 *   registered by the env dashboard.  Slot 'Q'/'q' is reserved to core.
 *   Maximum 8 slots; extra registrations are silently ignored.
 *
 * @param key    Keyboard character that selects this view.
 * @param label  Short label for the nav bar (max 15 chars).
 * @param fn     Render callback invoked once per redraw.
 */
void dashboard_register_slot(uint8_t key, const char* label, DashRenderFn fn);

/**
 * @brief Register a detail sub-view for a slot.
 *
 * @details After registration, pressing Enter while the slot is active
 *   opens the detail renderer.  Left/Right arrows navigate between
 *   sub-items; Q returns to the parent slot.
 *
 *   The detail nav bar replaces the normal slot bar entirely while active.
 *
 * @param slotKey   Key of the parent slot (must match a registered slot).
 * @param countFn   Callback returning the number of sub-items at runtime.
 *                  Returning 0 prevents entering detail mode.
 * @param renderFn  Render callback; call dashboard_detail_index() inside
 *                  to know which sub-item to display.
 */
void dashboard_register_detail(uint8_t slotKey, DashDetailCount countFn, DashDetailFn renderFn);

/**
 * @brief Return the 0-based index of the currently displayed detail sub-item.
 *
 * @details Only valid when called from within a DashDetailFn render callback.
 */
uint8_t dashboard_detail_index();

/**
 * @brief Push a timestamped event into the ring buffer.
 *
 * @param msg  Short event description (truncated at 55 chars).
 */
void dashboard_push_event(const char* msg);

/**
 * @brief Non-blocking update — keyboard handling + timed redraw.
 *
 * @details Must be called once per loop() iteration.
 */
void dashboard_update();


#else // !DEBUG_DASHBOARD — empty stubs

using DashRenderFn    = void (*)(void);
using DashDetailFn    = void (*)(void);
using DashDetailCount = uint8_t (*)(void);

inline void dClear()                                               {}
inline void dPre()                                                 {}
inline void dTop()                                                 {}
inline void dMid()                                                 {}
inline void dBot()                                                 {}
inline void dContent(const char*)                                  {}
inline void dContentAnsi(const char*, int)                         {}
inline void dLine(const char*, ...)                                {}
inline void dEmpty()                                               {}
inline void dashboard_render_events()                              {}
inline void dashboard_push_log(const char*)                        {}
inline void dashboard_render_log()                                 {}
inline void fmtUptime(char*, size_t)                               {}
inline void fmtEventTime(uint32_t, char*, size_t)                  {}

inline void    dashboard_setup()                                                          {}
inline void    dashboard_register_slot(uint8_t, const char*, DashRenderFn)               {}
inline void    dashboard_register_detail(uint8_t, DashDetailCount, DashDetailFn)         {}
inline uint8_t dashboard_detail_index()                                                  { return 0; }
inline void    dashboard_push_event(const char*)                                         {}
inline void    dashboard_update()                                                        {}

#endif // DEBUG_DASHBOARD

// EOF dashboard.h
