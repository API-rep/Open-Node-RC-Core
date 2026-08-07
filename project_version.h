/**
 * @file  project_version.h
 * @brief Project contract version — embedded in every ComBus handshake frame.
 *
 * @details
 * Hand-maintained single source of truth for the project contract version.
 *
 * Read at build time by scripts/combus_md5.py (extra_script), which embeds
 * PROJECT_VERSION_MAJOR and PROJECT_VERSION_MINOR into
 * combus_handshake_md5.h. The firmware never re-reads this file at runtime.
 *
 * Wire mapping (combus_handshake_md5.h):
 *   - kCombusWireVersionMajor = PROJECT_VERSION_MAJOR
 *   - kCombusWireVersionMinor = PROJECT_VERSION_MINOR
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
 *
 * --------------------------------------------------------------------------
 * DO NOT RENAME the constants PROJECT_VERSION_MAJOR and PROJECT_VERSION_MINOR.
 *
 *   scripts/combus_md5.py parses them by exact identifier name via a strict
 *   regex. Renaming either constant will silently produce an MD5 over an
 *   empty payload (the script will fail with a clear FATAL message, but the
 *   combus handshake will be broken until fixed).
 *
 *   Reformatting, whitespace changes, comment additions, alignment edits,
 *   the trailing `u` suffix on the literal — all safe. Only the constant
 *   NAMES are load-bearing.
 *
 *   If you ever need to rename them, update both:
 *     1. this file
 *     2. the regex in scripts/combus_md5.py
 * --------------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>

namespace project {

/**
 * @brief Project contract major version.
 *
 * Increment on non-backward-compatible contract changes.
 */
static constexpr uint8_t PROJECT_VERSION_MAJOR = 0u;

/**
 * @brief Project contract minor version.
 *
 * Increment on backward-compatible contract extensions.
 */
static constexpr uint8_t PROJECT_VERSION_MINOR = 1u;

}  // namespace project

// EOF project_version.h
