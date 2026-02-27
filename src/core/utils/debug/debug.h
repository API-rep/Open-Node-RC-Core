/******************************************************************************
 * @file debug.h
 * @brief Unified debug system — serial bootstrap, log levels and log wrappers
 *
 * @details Build flags from platformio.ini are ingested once in section 1,
 *   immediately mapped to typed C++ constexpr constants. All subsequent logic
 *   uses standard C++17 (if constexpr, constexpr bool) with no preprocessor
 *   guards around code blocks.
 *
 *   Activation via platformio.ini build_flags:
 *     -D LOG_LEVEL=3          ; 0=off 1=err 2=warn 3=info(default) 4=dbg
 *     -D DEBUG_HW             ; enable hardware log wrappers
 *     -D DEBUG_SYSTEM         ; enable system log wrappers
 *     -D DEBUG_INPUT          ; enable input log wrappers
 *     -D DEBUG_COMBUS         ; enable combus log wrappers
 *     -D DEBUG_ALL            ; shortcut — enables all module wrappers
 *****************************************************************************/
#pragma once

#include <Arduino.h>


// =============================================================================
// 1. BUILD FLAG INGESTION
// =============================================================================

// This section is the only place where #if / #define are used.
// Each build flag is read once and immediately mapped to a typed C++ constant.
// All downstream code uses those constants exclusively.


// --- 1.1 Log level constants ---

  // Severity levels — used as template arguments and in if constexpr comparisons.
constexpr uint8_t LogNone  = 0;  ///< No output — serial fully disabled.
constexpr uint8_t LogError = 1;  ///< Errors only.
constexpr uint8_t LogWarn  = 2;  ///< Errors and warnings.
constexpr uint8_t LogInfo  = 3;  ///< Normal verbosity (default).
constexpr uint8_t LogDebug = 4;  ///< Full trace output.


  // Active verbosity threshold — calls above this level are stripped at compile time.
  // Injected as an integer literal: -D LOG_LEVEL=3
#ifndef LOG_LEVEL
  #define LOG_LEVEL 3
#endif

constexpr uint8_t LogLevel = (uint8_t)(LOG_LEVEL);


// --- 1.2 Module gate constants ---

// DEBUG_ALL expands to all module gates.

#if defined(DEBUG_ALL) || defined(DEBUG_HW)
  constexpr bool DbgHw = true;
#else
  constexpr bool DbgHw = false;
#endif

#if defined(DEBUG_ALL) || defined(DEBUG_INPUT)
  constexpr bool DbgInput = true;
#else
  constexpr bool DbgInput = false;
#endif

#if defined(DEBUG_ALL) || defined(DEBUG_SYSTEM)
  constexpr bool DbgSystem = true;
#else
  constexpr bool DbgSystem = false;
#endif

#if defined(DEBUG_ALL) || defined(DEBUG_COMBUS)
  constexpr bool DbgCombus = true;
#else
  constexpr bool DbgCombus = false;
#endif


// --- 1.3 Serial configuration ---

/// Baud rate for the debug serial monitor.
/// Injected via: -D DEBUG_MONITOR_BAUD=${env.monitor_speed}
#ifndef DEBUG_MONITOR_BAUD
  #define DEBUG_MONITOR_BAUD 115200
#endif
constexpr uint32_t MonitorBaud = (uint32_t)(DEBUG_MONITOR_BAUD);

/// Enable ANSI escape sequences — set to 0 when monitor does not support them.
/// Injected via: -D DEBUG_SERIAL_ANSI=0
#ifndef DEBUG_SERIAL_ANSI
  #define DEBUG_SERIAL_ANSI 1
#endif
constexpr bool SerialAnsi = (bool)(DEBUG_SERIAL_ANSI);

/// Clear screen before first log line to hide ESP32 ROM boot noise.
/// Injected via: -D DEBUG_SERIAL_CLEAR_ON_INIT=0
#ifndef DEBUG_SERIAL_CLEAR_ON_INIT
  #define DEBUG_SERIAL_CLEAR_ON_INIT 1
#endif
constexpr bool SerialClearOnInit = (bool)(DEBUG_SERIAL_CLEAR_ON_INIT);


// =============================================================================
// 2. CORE LOG TEMPLATE
// =============================================================================

/**
 * @brief Core log writer — not intended for direct call-site use.
 *
 * @details The if constexpr condition is evaluated at compile time.
 *   When false, the entire function body is physically removed from the binary.
 *
 * @tparam Level         Severity level of this call (LogError … LogDebug)
 * @tparam ModuleEnabled Per-module gate (DbgHw, DbgInput, etc.) — false
 *                       strips the call regardless of LogLevel
 */
template<uint8_t Level, bool ModuleEnabled, typename... Args>
inline void log_impl(const char* fmt, Args... args) {
  if constexpr (Level <= LogLevel && ModuleEnabled) {
    Serial.printf(fmt, args...);
  }
}


// =============================================================================
// 3. GENERIC LOG WRAPPERS  (always active — level-filtered only)
// =============================================================================

// Use these for outputs not tied to a specific module theme.

	/// Print an error-log-level message.
template<typename... Args>
inline void log_err(const char* fmt, Args... args) {
  log_impl<LogError, true>(fmt, args...);
}

	/// Print a warning-log-level message.
template<typename... Args>
inline void log_warn(const char* fmt, Args... args) {
  log_impl<LogWarn, true>(fmt, args...);
}

	/// Print an info-log-level message.
template<typename... Args>
inline void log_info(const char* fmt, Args... args) {
  log_impl<LogInfo, true>(fmt, args...);
}

	/// Print a debug-log-level message.
template<typename... Args>
inline void log_dbg(const char* fmt, Args... args) {
  log_impl<LogDebug, true>(fmt, args...);
}


// =============================================================================
// 4. HARDWARE LOG WRAPPERS  (gated by DEBUG_HW)
// =============================================================================

  /// Print a hardware error-log-level message while DEBUG_HW is set.
template<typename... Args>
inline void hw_log_err(const char* fmt, Args... args) {
  log_impl<LogError, DbgHw>(fmt, args...);
}

	/// Print a hardware warning-log-level message while DEBUG_HW is set.
template<typename... Args>
inline void hw_log_warn(const char* fmt, Args... args) {
  log_impl<LogWarn, DbgHw>(fmt, args...);
}

	/// Print a hardware info-log-level message while DEBUG_HW is set.
template<typename... Args>
inline void hw_log_info(const char* fmt, Args... args) {
  log_impl<LogInfo, DbgHw>(fmt, args...);
}

	/// Print a hardware debug-log-level message while DEBUG_HW is set.
template<typename... Args>
inline void hw_log_dbg(const char* fmt, Args... args) {
  log_impl<LogDebug, DbgHw>(fmt, args...);
}


// =============================================================================
// 5. INPUT LOG WRAPPERS  (gated by DEBUG_INPUT)
// =============================================================================

  /// Print an input error-log-level message while DEBUG_INPUT is set.
template<typename... Args>
inline void input_log_err(const char* fmt, Args... args) {
  log_impl<LogError, DbgInput>(fmt, args...);
}

	/// Print an input warning-log-level message while DEBUG_INPUT is set.
template<typename... Args>
inline void input_log_warn(const char* fmt, Args... args) {
  log_impl<LogWarn, DbgInput>(fmt, args...);
}

	/// Print an input info-log-level message while DEBUG_INPUT is set.
template<typename... Args>
inline void input_log_info(const char* fmt, Args... args) {
  log_impl<LogInfo, DbgInput>(fmt, args...);
}

	/// Print an input debug-log-level message while DEBUG_INPUT is set.
template<typename... Args>
inline void input_log_dbg(const char* fmt, Args... args) {
  log_impl<LogDebug, DbgInput>(fmt, args...);
}


// =============================================================================
// 6. SYSTEM LOG WRAPPERS  (gated by DEBUG_SYSTEM)
// =============================================================================

  /// Print a system error-log-level message while DEBUG_SYSTEM is set.
template<typename... Args>
inline void sys_log_err(const char* fmt, Args... args) {
  log_impl<LogError, DbgSystem>(fmt, args...);
}

	/// Print a system warning-log-level message while DEBUG_SYSTEM is set.
template<typename... Args>
inline void sys_log_warn(const char* fmt, Args... args) {
  log_impl<LogWarn, DbgSystem>(fmt, args...);
}

	/// Print a system info-log-level message while DEBUG_SYSTEM is set.
template<typename... Args>
inline void sys_log_info(const char* fmt, Args... args) {
  log_impl<LogInfo, DbgSystem>(fmt, args...);
}

	/// Print a system debug-log-level message while DEBUG_SYSTEM is set.
template<typename... Args>
inline void sys_log_dbg(const char* fmt, Args... args) {
  log_impl<LogDebug, DbgSystem>(fmt, args...);
}


// =============================================================================
// 7. COMBUS LOG WRAPPERS  (gated by DEBUG_COMBUS)
// =============================================================================

  /// Print a combus error-log-level message while DEBUG_COMBUS is set.
template<typename... Args>
inline void combus_log_err(const char* fmt, Args... args) {
  log_impl<LogError, DbgCombus>(fmt, args...);
}

	/// Print a combus warning-log-level message while DEBUG_COMBUS is set.
template<typename... Args>
inline void combus_log_warn(const char* fmt, Args... args) {
  log_impl<LogWarn, DbgCombus>(fmt, args...);
}

	/// Print a combus info-log-level message while DEBUG_COMBUS is set.
template<typename... Args>
inline void combus_log_info(const char* fmt, Args... args) {
  log_impl<LogInfo, DbgCombus>(fmt, args...);
}

	/// Print a combus debug-log-level message while DEBUG_COMBUS is set.
template<typename... Args>
inline void combus_log_dbg(const char* fmt, Args... args) {
  log_impl<LogDebug, DbgCombus>(fmt, args...);
}


// =============================================================================
// 8. SERIAL BOOTSTRAP
// =============================================================================

/**
 * @brief Initialize debug serial monitor — safe to call multiple times.
 *
 * @details Uses if constexpr so the body is physically stripped when
 *   LogLevel == LogNone. No preprocessor guards required — pure C++17.
 */
void debugInit();

// EOF debug.h
