// =============================================================================
// combus_ids_remote_digital_common.inc
// Common umbrella for Digital REMOTE channel IDs — shared across ALL machine
// types (dumper_truck, future types).
//
// Always pulls in the Failsafe aggregator fragment, which itself dispatches
// its internal contributors (e.g. HAS_VBAT_FAILSAFE → FAILSAFE_VBAT). If no
// internal contributor is active, the Failsafe fragment still resolves to
// its minimal core (FAILSAFE + FAILSAFE_END).
//
// It is included by TYPE-level fragments such as:
//   src/core/config/machines/dumper_truck/combus/combus_ids_remote_digital.inc
// =============================================================================

#include <core/system/failsafe/combus_ids_digital_failsafe.inc>

// EOF combus_ids_remote_digital_common.inc
