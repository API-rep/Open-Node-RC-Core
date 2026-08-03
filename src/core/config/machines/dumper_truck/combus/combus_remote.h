/******************************************************************************
 * @file  combus_remote.h
 * @brief Dumper-truck ComBus REMOTE runtime umbrella — TYPE-level runtime
 *        side of the REMOTE-only ComBus vocabulary.
 *
 * @details Mirror of combus_ids_remote.h, but for the RUNTIME side
 *   (externs, init hooks, future per-type ComBus runtime functions).
 *   combus_ids_remote.h provides the *vocabulary* (channel IDs);
 *   combus_remote.h provides the *runtime* hooks that operate on it.
 *
 *   Why a separate header?
 *     - combus_ids_remote.h is pure-token (enum values) — includable from
 *       anywhere, even unit-test stubs with no ComBus transport.
 *     - combus_remote.h pulls runtime state (NodeCom handle, RX ring
 *       buffer, future handshake FSM) — only meaningful on a real node.
 *
 *   Inclusion path: never include directly.  Consumers go through
 *   machine_type.h → <machine_type>_config.h, which #includes this file
 *   alongside combus_ids_remote.h.
 *
 *   Today (this revision): this header is a thin umbrella — it re-exports
 *   combus_ids_remote.h's declarations and owns the TYPE-level runtime
 *   identifiers listed below.  It exists so the dispatcher chain
 *   (machine_type.h → dumper_truck_config.h → combus_remote.{h,cpp}) is
 *   in place BEFORE the first REMOTE runtime hook needs to be added —
 *   the same way combus_handshake_rx/tx split pre-emptively landed
 *   before the actual handshake implementation.
 ******************************************************************************
 */
#pragma once


// =============================================================================
// 0. SELF — TYPE-level runtime identifiers
// =============================================================================
//
// Today: a single monotonic TYPE-level wire-version constant.  Future
// runtime hooks (remote-init, per-type keepalive, handshake FSM) will be
// added here as additional functions, never as new headers in the
// machine_config.h chain.
//
// The wire-version is the TYPE-local view of "what Remote vocabulary
// version does this firmware speak?".  Bump it when a new channel is
// added to the REMOTE .inc set.  The HASH of the .inc files (generated
// by scripts/combus_md5.py) is the wire-level identity; this constant
// is the human-readable companion that goes into logs / dashboards.

#include <stdint.h>

/// Dumper-truck REMOTE wire vocabulary version (bump when adding a
/// REMOTE channel to any .inc file).
static constexpr uint8_t kDumperTruckCombusRemoteWireVersion = 1u;


// =============================================================================
// 1. VOCABULARY RE-EXPORT
// =============================================================================
//
// combus_ids_remote.h defines AnalogComBusRemoteID / DigitalComBusRemoteID.
// We pull it here so consumers that #include combus_remote.h get BOTH the
// runtime constants above AND the REMOTE vocabulary in one shot.  This is
// the canonical pattern: TYPE-level consumers always include the umbrella,
// never the IDs header directly.

#include <core/config/machines/dumper_truck/combus/combus_ids_remote.h>


// EOF combus_remote.h