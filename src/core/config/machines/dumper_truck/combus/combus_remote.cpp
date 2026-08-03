/******************************************************************************
 * @file  combus_remote.cpp
 * @brief Dumper-truck ComBus REMOTE runtime — TYPE-level runtime definitions.
 *
 * @details Mirror of combus_ids_remote.h (vocabulary) for the runtime side.
 *   Today the only TYPE-level runtime constant is the wire-version
 *   (declared in combus_remote.h).  Future REMOTE runtime hooks will land
 *   here.
 *
 *   Existence rationale: a no-op .cpp is included by the build so that
 *   the linker resolves any future declared-but-not-yet-defined symbol
 *   gracefully, and so that `combus_remote.h`'s `static constexpr`
 *   constants have a TU to live in should they ever evolve into
 *   non-constexpr state.
 *
 *   Scope EXPLICITLY out of this revision:
 *     - per-type keepalive / heartbeat
 *     - per-type handshake FSM
 *     - per-type friend-cache entry pruning
 ******************************************************************************
 */

#include "combus_remote.h"

// (no definitions today — see header for rationale)