/******************************************************************************
 * @file switch.cpp
 * @brief Polling-based switch debounce — board config driven.
 *****************************************************************************/

#include "switch.h"

#include <Arduino.h>

// =============================================================================
// 1. MODULE STATE
// =============================================================================

static SwitchPort* s_switch = nullptr;

// =============================================================================
// 2. API
// =============================================================================

/**
 * @brief Register the board switch config and seed initial pin states.
 *
 * @details Called once during hardware init. `switch_init()` stores the
 *   `SwitchPort` pointer in the module-static `s_switch` and immediately
 *   reads each pin so that `state[i].confirmed` reflects the physical level
 *   before the first `switch_update()` call.
 *
 *   The caller owns the `SwitchPort` and its arrays for the lifetime of the
 *   program. Passing `nullptr`, or a container with null `cfg`/`state`
 *   pointers, is a silent no-op.
 *
 * @param port  Board-defined SwitchPort container (must outlive all calls).
 */

void switch_init(SwitchPort* port)
{
    s_switch = port;

      // cfg/state arrays must be valid.
    if (!port || !port->cfg || !port->state) {
      return;
    }

    for (uint8_t i = 0; i < port->count; i++) {
          // Skip unconfigured channels.
        if (port->cfg[i].pin < 0) continue;

          // Apply pull mode from board config.
        pinMode(port->cfg[i].pin, port->cfg[i].pullUp ? INPUT_PULLUP : INPUT_PULLDOWN);

          // Seed all three state fields from actual hardware level.
        bool initial               = digitalRead(port->cfg[i].pin);
        port->state[i].pending     = initial;
        port->state[i].pendingMs   = 0;
        port->state[i].confirmed   = initial;
    }
}



/**
 * @brief Poll all switches and commit debounced state changes.
 *
 * @details Must be called once per loop iteration. For each channel,
 *   any raw level change starts a debounce window (`debounceMs`). The
 *   new level is committed to `state[i].confirmed` only after the signal
 *   has been stable for the full window duration.
 *
 *   Channels with `pin < 0` are skipped silently. A zero `debounceMs`
 *   commits immediately on the first differing read.
 */

void switch_update()
{
      // Not initialised — nothing to do.
    if (!s_switch || !s_switch->cfg || !s_switch->state) return;

      // Snapshot current timestamp once per call.
    uint32_t now = millis();

    for (uint8_t i = 0; i < s_switch->count; i++) {
          // Skip unconfigured channels.
        if (s_switch->cfg[i].pin < 0) continue;

          // Sample raw pin level.
        bool raw = digitalRead(s_switch->cfg[i].pin);

          // Track the most-recent raw transition.
        if (raw != s_switch->state[i].pending) {
            s_switch->state[i].pending   = raw;
            s_switch->state[i].pendingMs = now;
        }

          // Commit once the level has been stable for debounceMs.
        uint16_t db     = s_switch->cfg[i].debounceMs;
        bool     stable = (db == 0) || ((now - s_switch->state[i].pendingMs) >= db);

        if (stable && s_switch->state[i].pending != s_switch->state[i].confirmed) {
            s_switch->state[i].confirmed = s_switch->state[i].pending;
        }
    }
}



/**
 * @brief Return the debounced level of switch @p idx.
 *
 * @param idx  Zero-based channel index.
 * @return     Confirmed `digitalRead()` level, or `false` if out of range.
 */

bool switch_read(uint8_t idx)
{
      // Bounds and init check.
    if (!s_switch || idx >= s_switch->count) return false;
      // Return last debounced level.
    return s_switch->state[idx].confirmed;
}



/**
 * @brief Return the number of registered switch channels.
 *
 * @return Channel count from the active `SwitchPort`, or 0 if not initialised.
 */

uint8_t switch_count()
{
      // 0 before switch_init().
    return s_switch ? s_switch->count : 0;
}

// EOF switch.cpp
