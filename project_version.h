/**
 * @file  project_version.h
 * @brief Project version — embedded in every ComBus handshake frame.
 *
 * @details Hand-maintained single source of truth for the wire-level project
 *   version.  Read at build time by scripts/combus_md5.py (extra_script),
 *   which embeds MAJOR/MINOR into combus_handshake_md5.h so the runtime
 *   never re-reads this file.
 *
 *   Wire mapping (combus_handshake_md5.h):
 *     - kCombusWireVersionMajor = PROJECT_VERSION_MAJOR
 *     - kCombusWireVersionMinor = PROJECT_VERSION_MINOR
 *
 *   Bump MAJOR when the wire payload layout / MD5 semantics change in a
 *   non-backward-compatible way.  Bump MINOR for additive changes (extra
 *   remote .inc files, new optional channels) that keep the wire protocol
 *   compatible.
 *
 *   Project version is kept at the repository root (not under include/)
 *   because it is read by scripts/combus_md5.py via Path("PROJECT_DIR") /
 *   "project_version.h" — promoting it to a peer of platformio.ini makes
 *   the build script path stable across header-tree refactors.
 */

#pragma once

#include <stdint.h>

namespace project {

/**
 * @brief Wire-level major version.
 *        Bump on non-backward-compatible handshake / wire-format changes.
 */
static constexpr uint8_t PROJECT_VERSION_MAJOR = 0u;

/**
 * @brief Wire-level minor version.
 *        Bump on additive (backward-compatible) changes only.
 */
static constexpr uint8_t PROJECT_VERSION_MINOR = 1u;

}  // namespace project

// EOF project_version.h