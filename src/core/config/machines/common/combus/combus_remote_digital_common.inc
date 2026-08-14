// =============================================================================
// combus_remote_digital_common.inc
// Common umbrella for Digital REMOTE channel init values — shared across ALL
// machine types.
//
// Pairs with `combus_ids_remote_digital_common.inc` (same role, but for
// channel init fragments consumed by `combus.cpp`).
//
// Always pulls in the Failsafe aggregator fragment, which itself dispatches
// its internal contributors (e.g. HAS_VBAT_FAILSAFE → vbat failsafe channel).
//
// It is included by TYPE-level fragments such as:
//   src/core/config/machines/dumper_truck/combus/combus_remote_digital.inc
// =============================================================================

#include <core/system/failsafe/combus_digital_failsafe.inc>

// EOF combus_remote_digital_common.inc
