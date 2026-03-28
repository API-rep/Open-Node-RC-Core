/******************************************************************************
 * @file vbat_sense.h
 * @brief Battery voltage sensing — core utility module.
 *
 * @details Voltage divider-based ADC sensing with sliding average,
 *   1S–6S auto-detection, and hysteresis re-arm logic.
 *   Activated by -D VBAT_SENSING=<TYPE> in platformio.ini (e.g. LIPO).
 *
 *   Channel count is driven by VBatSense.count (set by the board).
 *   When VBAT_SENSING is not defined, all functions are inline no-ops —
 *   no #ifdef required in callers.
 *****************************************************************************/
#pragma once

#include <struct/vbat_struct.h>

#ifdef VBAT_SENSING

#include <core/config/vbat/config.h>


// =============================================================================
// 1. API
// =============================================================================

	/// Initialize all sensing channels from a board.h config file.
void vbat_sense_init(VBatSense& sense);

	/// Checks and updates voltage sensing average and low-battery state.
bool vbat_sense_tick();

	/// Return the channel name (infoName from board config).
	/// Returns "---" if idx is out of range or module not initialized.
const char* vbat_name(uint8_t idx = 0);

	/// Return the battery chemistry type name (e.g. "LiPo").
const char* vbat_tech_name();

	/// Return the number of active sensing channels (0 if not initialized).
uint8_t vbat_channel_count();

	/// Return the last sliding-average voltage for channel idx (V).
	/// Returns 0.0 if idx is out of range or module is not initialized.
float vbat_voltage(uint8_t idx = 0);

	/// Return the battery voltage captured at init for channel idx (V).
	/// Used by the dashboard to show drift from the boot reading.
float vbat_voltage_at_init(uint8_t idx = 0);

	/// Return the auto-detected cell count for channel idx (1S–6S).
	/// Detection runs once at init from the measured voltage; returns 0 if unknown.
uint8_t vbat_cells(uint8_t idx = 0);

	/// Return true when channel idx is currently below its low-battery cutoff.
	/// Latches until voltage recovers above cutoff + hysteresis (re-arm threshold).
bool vbat_is_low(uint8_t idx = 0);

	/// Return true when channel idx was disabled at init (no voltage detected — not wired).
	/// Returns true for out-of-range idx.
bool vbat_is_disabled(uint8_t idx = 0);

	/// Return a read-only pointer to the hardware/threshold config for channel idx.
	/// Returns nullptr if idx is out of range or module not initialized.
	/// Used by the dashboard detail view to display init-time parameters.
const VBatSenseConfig* vbat_cfg(uint8_t idx = 0);

#else // VBAT_SENSING not defined

// =============================================================================
// 2. NO-OP STUBS — no #ifdef needed in callers
// =============================================================================

	/// Dummy symbols so unconditional call sites compile without #ifdef.
inline void    vbat_sense_init(VBatSense&)     {}
inline bool    vbat_sense_tick()           { return false; }
inline uint8_t vbat_channel_count()       { return 0;     }
inline const char* vbat_name(uint8_t = 0) { return "---"; }
inline const char* vbat_tech_name()       { return "---"; }
inline float   vbat_voltage(uint8_t = 0)         { return 0.0f;  }
inline float   vbat_voltage_at_init(uint8_t = 0) { return 0.0f;  }
inline uint8_t vbat_cells(uint8_t = 0)           { return 0;     }
inline bool    vbat_is_low(uint8_t = 0)                    { return false;   }
inline bool    vbat_is_disabled(uint8_t = 0)               { return true;    }
inline const VBatSenseConfig* vbat_cfg(uint8_t = 0)        { return nullptr; }

#endif // VBAT_SENSING

// EOF vbat_sense.h
