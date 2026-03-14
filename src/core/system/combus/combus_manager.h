/**
 * @file combus_manager.h
 * @brief ComBus utility and debug hooks
 */

#pragma once

#include <struct/combus_struct.h>
#include <core/system/debug/debug.h>

/**
 * @brief Reset ComBus drive flags according to active input mapping
 * @param bus Reference to the main communication bus structure
 */
void resetComBusDriveFlags(ComBus &bus);

/**
 * @brief ComBus frame reception watchdog.
 *
 * @details Compares millis() against combus.lastFrameMs (set by combus_frame_apply).
 * Resets all isDrived flags via resetComBusDriveFlags() if no valid frame has
 * been applied within timeoutMs. Must be called once per loop, before the
 * isDrived check / failsafe logic.
 *
 * @param bus       Target ComBus instance.
 * @param timeoutMs Maximum acceptable gap between two successful frame applies.
 */
void combus_watchdog(ComBus &bus, uint32_t timeoutMs);


// EOF combus_manager.h
