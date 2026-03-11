/******************************************************************************
 * @file vbat_sense.cpp
 * @brief Battery voltage sensing — core utility module implementation.
 *
 * @details Voltage divider formula:
 *   vPin  = analogRead(pin) * adcRefVoltage / ADC_MAX_COUNT
 *   vBat  = vPin * (hsResOhm + lsResOhm) / lsResOhm + diodeDrop
 *
 *   Sliding average (SamplingDepth samples) per channel.
 *   Cell count (1S–6S) auto-detected at init from measured voltage.
 *   All channels indexed by VBatChannel enum (board config).
 *****************************************************************************/

#ifdef VBAT_SENSING

#include "vbat_sense.h"

#include <Arduino.h>
#include <const.h>
#include <core/system/debug/debug.h>


	/// Vbat sensing datas — set once by vbat_init(), used by all accessors and tick.
static VBatSense* vBatSense;


// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

/**
 * @brief Read instantaneous battery voltage (V) for channel idx.
 *
 * @details Applies the voltage-divider scaling and diode-drop correction
 *   to a single raw ADC read. No averaging — one sample only.
 *
 * @param idx  Channel index (0 for first channel).
 * @return Instantaneous battery voltage in volts.
 */

static float readInstantVoltage(uint8_t idx) {
	  // --- 1. Raw ADC to GPIO voltage ---
  float vPin = (float)analogRead(vBatSense->cfg[idx].pin) * vBatSense->cfg[idx].adcRefVoltage / (float)ADC_MAX_COUNT;  // voltage at GPIO pin (V)

	  // --- 2. GPIO voltage to battery voltage (divider + diode) ---
  float scale = (float)(vBatSense->cfg[idx].hsResOhm + vBatSense->cfg[idx].lsResOhm) / (float)vBatSense->cfg[idx].lsResOhm;  // voltage divider ratio

  return vPin * scale + vBatSense->cfg[idx].diodeDrop;
}



/**
 * @brief Read vbat channel idx voltage, shift the sample buffer, and return
 *   the updated average voltage.
 *
 * @details Inserts a fresh readInstantVoltage() sample at index 0 (oldest
 *   sample drops off the end), then averages all SamplingDepth entries.
 *
 * @param idx  Channel index (0-based).
 * @return Updated sliding-average voltage in volts.
 */

static float updateAverage(uint8_t idx) {
	  // --- 1. Shift raw val buffer (newest at index 0) ---
  for (uint8_t i = SamplingDepth - 1; i > 0; i--) {
    vBatSense->state[idx].rawVals[i] = vBatSense->state[idx].rawVals[i - 1];
  }

    // --- 2. Insert new sample at index 0 and compute average ---
  vBatSense->state[idx].rawVals[0] = readInstantVoltage(idx);

	  // --- 3. Compute average ---
  float sum = 0.0f; // accumulator
  for (uint8_t i = 0; i < SamplingDepth; i++) {
    sum += vBatSense->state[idx].rawVals[i];
  }
    // --- 4. Return average ---
  return sum / SamplingDepth;
}



// =============================================================================
// 2. INIT
// =============================================================================

/**
 * @brief Initialize all sensing channels from a board.h config file.
 *
 * @details Stores the board container pointer, then for each channel:
 *   1. Validates adcRefVoltage — halts the system (FATAL) if zero or negative.
 *   2. Configures the ADC GPIO pin as INPUT.
 *   2.5. Reads one instant voltage sample: if below 0.5 V (pull-down = not wired),
 *        marks state.disabled and skips the channel silently.
 *   3. Seeds the sliding average buffer with SamplingDepth real ADC reads so
 *      the first vbat_tick() result is immediately stable.
 *   4. Auto-detects cell count (1S–6S) by comparing the initial averaged voltage
 *      against chargedVolt thresholds spaced at 0.5-cell intervals.
 *
 *   Must be called once at startup before vbat_tick().
 *   Safe to call with sense.count == 0 (no channels configured, no-op).
 *
 * @param sense  Board-defined VBatSense container (cfg array + state array pre-wired).
 */
void vbat_init(VBatSense& sense) {
  vBatSense = &sense;

  for (uint8_t idx = 0; idx < vBatSense->count; idx++) {

	    // --- 1. Validate board logic voltage reference ---
    if (vBatSense->cfg[idx].adcRefVoltage <= 0.0f) {
      hw_log_err("    [BAT] FATAL: \"%s\" adcRefVoltage not set — system halted.\n", vBatSense->cfg[idx].infoName);
      while(1);
    }

	    // --- 2. Configure ADC pin ---
    pinMode(vBatSense->cfg[idx].pin, INPUT);

	    // --- 2.5. Disable check — pull-down (~0 V) means channel not wired ---
    if (readInstantVoltage(idx) < 0.5f) {
      vBatSense->state[idx].disabled = true;
      hw_log_info("    [BAT] \"%s\" — no voltage detected, sensing disabled.\n", vBatSense->cfg[idx].infoName);
      continue;
    }

	    // --- 3. Seed the sliding average buffer ---
    for (uint8_t s = 0; s < SamplingDepth; s++) {
      vBatSense->state[idx].rawVals[s] = readInstantVoltage(idx);
    }

    vBatSense->state[idx].voltage        = updateAverage(idx);
    vBatSense->state[idx].voltageAtInit   = vBatSense->state[idx].voltage;

	    // --- 4. Cell count detection (1S–6S) ---
    uint8_t n = 1; // cell count candidate, incremented until voltage fits

      // Partition at chargedVolt * (N + 0.5): midpoint between N and N+1 cell max
    while (n < 6 && vBatSense->state[idx].voltage > vBatSense->cfg[idx].chargedVolt * ((float)n + 0.5f)) {
      n++;
    }
      // set detected cell count in idx state register
    vBatSense->state[idx].cells = n;
    hw_log_info("    [BAT] \"%s\" init: %dS detected — %.2f V\n", vBatSense->cfg[idx].infoName, vBatSense->state[idx].cells, vBatSense->state[idx].voltage);

    vBatSense->state[idx].lastTickMs = millis();
  }
}


// =============================================================================
// 3. RUNTIME
// =============================================================================

/**
 * @brief Advance all sensing channels — call once per loop() iteration.
 *
 * @details For each channel independently:
 *   1. Skips the channel if its interval (cfg.intervalMs) has not elapsed yet.
 *   2. Updates the sliding average buffer with a new ADC read and recomputes
 *      the averaged voltage.
 *   3. Evaluates the low-battery condition with hysteresis:
 *      - Trips isLow when voltage < cutoffVolt * cells.
 *      - Re-arms (clears isLow) only when voltage >= (cutoffVolt + hysteresis) * cells.
 *      This prevents rapid toggling near the threshold.
 *   4. Logs a warning on trip and an info on recovery.
 *
 *   Returns immediately (false) if vbat_init() has not been called yet.
 *
 * @return True when any channel’s isLow state has changed this tick.
 */
bool vbat_tick() {
  if (!vBatSense) {
    return false;
  }

  bool anyChanged = false;           // true if any channel's isLow changed this tick
  unsigned long now = millis();      // timestamp snapshot for all channels this tick

  for (uint8_t i = 0; i < vBatSense->count; i++) {
    if (vBatSense->state[i].disabled) continue;

	    // --- 1. Per-channel interval guard ---
    if ((now - vBatSense->state[i].lastTickMs) < (unsigned long)vBatSense->cfg[i].intervalMs) {
      continue;
    }
    vBatSense->state[i].lastTickMs = now;

	    // --- 2. Update voltage average ---
    bool prevIsLow = vBatSense->state[i].isLow;  // isLow before update, for change detection
    vBatSense->state[i].voltage = updateAverage(i);

	    // --- 3. Low-battery detection with hysteresis ---
    float cutoff = vBatSense->cfg[i].cutoffVolt * (float)vBatSense->state[i].cells;  // trip threshold (V)
    float reArm  = (vBatSense->cfg[i].cutoffVolt + vBatSense->cfg[i].hysteresis) * (float)vBatSense->state[i].cells; // re-arm threshold (V)

    if (!vBatSense->state[i].isLow && vBatSense->state[i].voltage < cutoff) {
      vBatSense->state[i].isLow = true;
      hw_log_dbg("[HW][BAT] \"%s\" low! %.2f V < %.2f V\n", vBatSense->cfg[i].infoName, vBatSense->state[i].voltage, cutoff);
    }
    else if (vBatSense->state[i].isLow && vBatSense->state[i].voltage >= reArm) {
      vBatSense->state[i].isLow = false;
      hw_log_dbg("[HW][BAT] \"%s\" recovered: %.2f V >= %.2f V\n", vBatSense->cfg[i].infoName, vBatSense->state[i].voltage, reArm);
    }

    anyChanged |= (vBatSense->state[i].isLow != prevIsLow);
  }

  return anyChanged;
}


// =============================================================================
// 4. ACCESSORS
// =============================================================================

/**
 * @brief Return the number of active sensing channels.
 *
 * @details Reflects VBatSense.count as stored by vbat_init().
 *   Returns 0 if vbat_init() has not been called.
 *
 * @return Active channel count, or 0 if not initialized.
 */

uint8_t vbat_channel_count() { return vBatSense ? vBatSense->count : 0; }

/**
 * @brief Return the battery chemistry type name (compile-time string from profile).
 */
const char* vbat_tech_name() { return VBatTechName; }

/**
 * @brief Return the channel name from board config.
 */
const char* vbat_name(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count && vBatSense->cfg[idx].infoName)
         ? vBatSense->cfg[idx].infoName : "---";
}



/**
 * @brief Return the last sliding-average voltage for channel idx (V).
 *
 * @details The value is updated on every successful vbat_tick() interval.
 *   Averaging depth is SamplingDepth (defined in core/config/vbat/config.h).
 *   Defaults to channel 0 when called without argument.
 *
 * @param idx  Channel index (0-based). Defaults to 0.
 * @return Averaged voltage in volts, or 0.0 if idx is out of range.
 */

float vbat_voltage(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count) ? vBatSense->state[idx].voltage : 0.0f;
}



/**
 * @brief Return the auto-detected cell count for channel idx (1S–6S).
 *
 * @details Cell count is detected once during vbat_init() by comparing the
 *   initial averaged voltage against chargedVolt * (N + 0.5) thresholds.
 *   It is not updated at runtime — reconnecting a different pack requires
 *   a new vbat_init() call.
 *   Defaults to channel 0 when called without argument.
 *
 * @param idx  Channel index (0-based). Defaults to 0.
 * @return Detected cell count (1–6), or 0 if idx is out of range.
 */

uint8_t vbat_cells(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count) ? vBatSense->state[idx].cells : 0;
}



/**
 * @brief Return true when channel idx is below its low-battery cutoff.
 *
 * @details The flag latches true when voltage < cutoffVolt * cells.
 *   It clears only when voltage recovers above (cutoffVolt + hysteresis) * cells,
 *   preventing oscillation near the threshold.
 *   Defaults to channel 0 when called without argument.
 *
 * @param idx  Channel index (0-based). Defaults to 0.
 * @return True when the channel is in low-battery state, false otherwise
 *   or if idx is out of range.
 */

bool vbat_is_low(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count) ? vBatSense->state[idx].isLow : false;
}

bool vbat_is_disabled(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count) ? vBatSense->state[idx].disabled : true;
}

/**
 * @brief Return the battery voltage captured at init for channel idx (V).
 *
 * @details Stored once by vbat_init() right after the initial sliding-average
 *   seed. Used by the dashboard detail view to show voltage drift vs boot.
 *
 * @param idx  Channel index (0-based). Defaults to 0.
 * @return Init voltage in volts, or 0.0 if idx is out of range.
 */
float vbat_voltage_at_init(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count) ? vBatSense->state[idx].voltageAtInit : 0.0f;
}

const VBatSenseConfig* vbat_cfg(uint8_t idx) {
  return (vBatSense && idx < vBatSense->count) ? &vBatSense->cfg[idx] : nullptr;
}

#endif // VBAT_SENSING

// EOF vbat_sense.cpp
