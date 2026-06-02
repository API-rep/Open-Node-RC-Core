/**
 * @file combus_manager.h
 * @deprecated Replaced by sys_manager.h (sys_manager_update / sys_manager_reset).
 *
 * @details resetComBusDriveFlags and combus_watchdog are removed.
 *   isDrived is now a single flag on ComBus, pre-cleared by sys_manager_reset()
 *   each loop cycle and re-asserted by each active physical input source.
 */

#pragma once

// Intentionally empty — all functionality migrated to sys_manager.h.

// EOF combus_manager.h
