/**
 * @file  project_version.h
 * @brief Project contract version — embedded in every ComBus handshake frame.
 *
 * @details
 * Hand-maintained single source of truth for the project contract version.
 *
 * Read at build time by scripts/combus_md5.py (extra_script), which scans
 * src/core/ for any folder containing both combus_ids_remote_analog.inc
 * and combus_ids_remote_digital.inc and emits a `combus_ids_remote_md5.h`
 * next to each pair.  Each generated header carries PROJECT_VERSION_MAJOR
 * and PROJECT_VERSION_MINOR embedded under combus::wire::kProjectVersion*.
 * The firmware never re-reads this file at runtime.
 *
 * Wire mapping (combus_ids_remote_md5.h):
 *   - combus::wire::kProjectVersionMajor = PROJECT_VERSION_MAJOR
 *   - combus::wire::kProjectVersionMinor = PROJECT_VERSION_MINOR

 *
 * This version defines the compatibility contract between communicating
 * nodes. It is intentionally broader than the wire format alone and covers
 * every rule both peers must interpret identically.
 *
 * Bump MAJOR when the project contract changes in a
 * non-backward-compatible way.
 *
 * Bump MINOR when extending the project contract while preserving
 * backward compatibility.
 *
 * The ComBus MD5 identifies the exact compiled ComBus definition within
 * a given contract version.
 *
 * Build history, patch revisions and releases are intentionally handled
 * by the Git repository (tags and commits) rather than by this version.
 *
 * This file is kept at the repository root (not under include/) because it
 * is read directly by scripts/combus_md5.py via
 * Path("PROJECT_DIR") / "project_version.h", making its location stable
 * across future header-tree refactors.
 */

#pragma once

#include <stdint.h>

namespace project {

/**
 * @brief Project contract major version.
 *
 * Increment on non-backward-compatible contract changes.
 * DO NOT RENAME the constants
 */
static constexpr uint8_t PROJECT_VERSION_MAJOR = 0u;

/**
 * @brief Project contract minor version.
 *
 * Increment on backward-compatible contract extensions.
 * DO NOT RENAME the constants
 */
static constexpr uint8_t PROJECT_VERSION_MINOR = 1u;

}  // namespace project

// EOF project_version.h
