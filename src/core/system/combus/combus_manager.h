/**
 * @file combus_manager.h
 * @deprecated Replaced by sys_manager.h (sys_manager_update / sys_manager_reset).
 *
 * @details Chantier 12.6 (cleanup): the historical `bus.isDrived` flag
 *   has been REMOVED from the `ComBus` struct (see `combus_defs.h`).
 *   Link health is now monitored per-channel by independent contributors
 *   (PS4_DS4_BT_LINK_LOST, UART_LINK_LOST, ...) aggregated by
 *   `remote_link_fallback_chain.cpp` into `REMOTE_LINK_LOST`.  See
 *   `doc/failsafe_module.md` §12.5 (new design) and §12.6 (cleanup).
 *
 *   resetComBusDriveFlags and combus_watchdog are removed.  The
 *   `sys_manager_reset()` function is kept as a no-op stub for source-
 *   level compatibility with out-of-tree callers.
 */

#pragma once

// Intentionally empty — all functionality migrated to sys_manager.h.

// EOF combus_manager.h