/**
 * @file  project_version.h
 * @brief Project contract version — embedded in every ComBus handshake frame.
 *
 * @details
 * Hand-maintained single source of truth for the project contract version.
 *
 * Read at build time by the ComBus Builder (scripts/combus_builder/md5.py,
 * read_project_version()), which writes the two numbers in the generated
 * combus_wire_common.h under combus::wire::kProjectVersionMajor / Minor.
 * The firmware never includes or re-reads this file.
 *
 * Wire mapping (combus_wire_common.h):
 *   - combus::wire::kProjectVersionMajor = PROJECT_VERSION_MAJOR
 *   - combus::wire::kProjectVersionMinor = PROJECT_VERSION_MINOR
 *   - combus::wire::kProjectVersionDev   = PROJECT_VERSION_DEV
 *
 * The builder parses the three constants below with a regular expression
 * (`PROJECT_VERSION_MAJOR = <n>u;`, each in 0..255): keep that form.
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
 * is read directly by the ComBus Builder via
 * Path(PROJECT_DIR) / "project_version.h", making its location stable
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

/**
 * @brief Project contract dev version: the pool of candidate channels.
 *
 * Increment when the candidate channels (`candidate: true` in a .cb) change;
 * reset to 0 when they are promoted, at the next MAJOR.  Logged by the
 * handshake, never compared: the next MD5 decides whether two nodes share
 * their candidates.
 * DO NOT RENAME the constants
 */
static constexpr uint8_t PROJECT_VERSION_DEV = 0u;

}  // namespace project

// EOF project_version.h
