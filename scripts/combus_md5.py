#!/usr/bin/env python3
"""
combus_md5.py — PlatformIO extra_script.

Recursively resolves #include directives inside .inc files to compute
a stable MD5 fingerprint of the *effective* ComBus configuration.

Discovery rule:
  Scans src/core/ for folders containing BOTH:
    combus_ids_remote_analog.inc
    combus_ids_remote_digital.inc
  For each such folder, emits:
    combus_ids_remote_md5.h

Hash input (deterministic):
  1. PROJECT_VERSION_MAJOR / MINOR (from project_version.h)
  2. For each .inc of the pair (sorted):
       - relative path (POSIX)
       - fully resolved content (all #include expanded, #ifdef evaluated)
"""

import hashlib
import os
import re
from pathlib import Path

Import("env")  # noqa: F821  — SCons injects this

# =============================================================================
# CONFIG
# =============================================================================

PROJECT_ROOT = Path(env["PROJECT_DIR"])
SRC_ROOT = PROJECT_ROOT / "src"
SCAN_ROOT = SRC_ROOT / "core"
VERSION_PATH = PROJECT_ROOT / "project_version.h"

PAIR_ANALOG = "combus_ids_remote_analog.inc"
PAIR_DIGITAL = "combus_ids_remote_digital.inc"

# Search paths for #include resolution (relative to PROJECT_ROOT)
INCLUDE_PATHS = [
    SRC_ROOT,           # #include <core/...>  → src/core/...
    PROJECT_ROOT,       # #include <...>       → project root
]

# =============================================================================
# UTILS
# =============================================================================

def log(*args):
    """Safe ASCII logger for PlatformIO."""
    out = []
    for a in args:
        s = str(a)
        if isinstance(a, Path):
            try:
                s = a.relative_to(PROJECT_ROOT).as_posix()
            except ValueError:
                s = a.as_posix()
        out.append(s.encode("ascii", errors="replace").decode("ascii"))
    print("[combus_md5]", *out)


def read_text(path: Path) -> str:
    """Read file as UTF-8, return empty string on failure."""
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return ""


# =============================================================================
# 1. PROJECT VERSION
# =============================================================================

def extract_version(path: Path) -> tuple[int, int]:
    text = read_text(path)
    if not text:
        log("FATAL: cannot read", path)
        env.Exit(1)  # noqa: F821

    m_major = re.search(
        r"(?:static\s+constexpr\s+\w+\s+|#define\s+)"
        r"PROJECT_VERSION_MAJOR\s*(?:=)?\s*(\d+)",
        text,
    )
    m_minor = re.search(
        r"(?:static\s+constexpr\s+\w+\s+|#define\s+)"
        r"PROJECT_VERSION_MINOR\s*(?:=)?\s*(\d+)",
        text,
    )
    if not (m_major and m_minor):
        log("FATAL: PROJECT_VERSION_{MAJOR,MINOR} not found in", path)
        env.Exit(1)

    return int(m_major.group(1)), int(m_minor.group(1))


VER_MAJOR, VER_MINOR = extract_version(VERSION_PATH)

# =============================================================================
# 2. DISCOVER .inc PAIRS
# =============================================================================

def find_pairs(root: Path) -> list[tuple[Path, list[Path]]]:
    """
    Return [(folder, [analog_path, digital_path]), ...] for every folder
    under `root` containing both PAIR_ANALOG and PAIR_DIGITAL.
    """
    pairs = []
    for dirpath, _, filenames in os.walk(root):
        if PAIR_ANALOG in filenames and PAIR_DIGITAL in filenames:
            folder = Path(dirpath)
            pairs.append((
                folder,
                [folder / PAIR_ANALOG, folder / PAIR_DIGITAL],
            ))
    pairs.sort(key=lambda kv: kv[0].as_posix())
    return pairs


PAIRS = find_pairs(SCAN_ROOT)
if not PAIRS:
    log("no .inc pairs found under", SCAN_ROOT, "— nothing to generate.")

# =============================================================================
# 3. EXTRACT -D FLAGS FROM BUILD ENVIRONMENT
# =============================================================================

def get_defined_flags(env) -> set[str]:
    """Return set of preprocessor symbols defined via -D in build_flags."""
    flags: set[str] = set()
    try:
        raw = env.GetProjectOption("build_flags", "")
    except Exception:
        return flags

    text = " ".join(str(x) for x in raw) if isinstance(raw, list) else str(raw)

    for m in re.finditer(r"-D\s*([A-Za-z_][A-Za-z0-9_]*)", text):
        flags.add(m.group(1))
    return flags


DEFINED_FLAGS = get_defined_flags(env)

# =============================================================================
# 4. RECURSIVE #include RESOLVER WITH MINIMAL PREPROCESSOR
# =============================================================================

_RE_INCLUDE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]')
_RE_IFDEF = re.compile(r"^\s*#\s*ifdef\s+([A-Za-z_][A-Za-z0-9_]*)")
_RE_ENDIF = re.compile(r"^\s*#\s*endif\b")


def resolve_include(name: str, base_dir: Path) -> Path | None:
    """
    Resolve an #include name to an absolute Path.
    Search order:
      1. Relative to base_dir (for #include "foo.inc")
      2. Each path in INCLUDE_PATHS (for #include <core/...>)
    """
    # 1. Relative to the current file's directory
    cand = base_dir / name
    if cand.is_file():
        return cand.resolve()

    # 2. Project search paths
    for prefix in INCLUDE_PATHS:
        cand = prefix / name
        if cand.is_file():
            return cand.resolve()

    return None


def resolve_file(path: Path, visited: set[Path], defined: set[str]) -> bytes:
    """
    Recursively resolve a file:
      - Expand #include directives transitively.
      - Evaluate #ifdef / #endif (single level, no nesting).
      - Skip already-visited files (cycle guard).

    Returns the resolved content as raw bytes.
    """
    try:
        real = path.resolve()
    except OSError:
        return b""

    if real in visited:
        return b""  # break cycle
    visited.add(real)

    text = read_text(path)
    if not text:
        return b""

    out = bytearray()
    lines = text.splitlines()
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]
        stripped = line.strip()

        # --- #ifdef FLAG ... #endif (single level) ---
        m_ifdef = _RE_IFDEF.match(stripped)
        if m_ifdef:
            flag = m_ifdef.group(1)
            # Find matching #endif
            j = i + 1
            while j < n and not _RE_ENDIF.match(lines[j].strip()):
                j += 1

            if flag in defined:
                # Keep block content (lines between #ifdef and #endif)
                for k in range(i + 1, j):
                    out.extend(_process_line(lines[k], path.parent, visited, defined))
            # else: drop entire block

            i = j + 1  # skip past #endif
            continue

        # --- Regular line (may contain #include) ---
        out.extend(_process_line(line, path.parent, visited, defined))
        i += 1

    return bytes(out)


def _process_line(line: str, base_dir: Path, visited: set[Path], defined: set[str]) -> bytes:
    """Process one source line: resolve #include or pass through."""
    m = _RE_INCLUDE.match(line.strip())
    if m:
        inc_path = resolve_include(m.group(1), base_dir)
        if inc_path is not None:
            return resolve_file(inc_path, visited, defined)
        # Unresolved include: keep directive verbatim (detectable mismatch)
    return (line + "\n").encode("utf-8")


# =============================================================================
# 5. HASH INPUT BUILDER
# =============================================================================

def build_hash_input(inc_files: list[Path]) -> bytes:
    """
    Build the canonical byte string to hash.

    Layout:
      "v<MAJOR>.<MINOR>\n"
      for each .inc (sorted by path):
        "<rel-path>\n"
        <resolved-content>\n
    """
    parts = [f"v{VER_MAJOR}.{VER_MINOR}\n".encode("ascii")]

    for p in sorted(inc_files):
        rel = p.relative_to(PROJECT_ROOT).as_posix()
        parts.append(rel.encode("ascii") + b"\n")

        visited: set[Path] = set()
        resolved = resolve_file(p, visited, DEFINED_FLAGS)
        parts.append(resolved + b"\n")

    return b"".join(parts)


# =============================================================================
# 6. EMIT GENERATED HEADERS
# =============================================================================

def emit_md5_header(folder: Path, inc_files: list[Path]) -> None:
    hash_in = build_hash_input(inc_files)
    md5_hex = hashlib.md5(hash_in).hexdigest()
    md5_bytes = ", ".join(f"0x{b:02X}u" for b in bytes.fromhex(md5_hex))

    rel_folder = folder.relative_to(PROJECT_ROOT).as_posix()

    header = f"""\
/* ============================================================================
 * combus_ids_remote_md5.h  —  AUTO-GENERATED by scripts/combus_md5.py
 *
 * DO NOT EDIT. Re-generated on every PlatformIO build. Gitignored.
 *
 * MD5 computed from:
 *   - PROJECT_VERSION_MAJOR / MINOR
 *   - {PAIR_ANALOG}
 *   - {PAIR_DIGITAL}
 *   - all recursively resolved #include contents
 *   - evaluated #ifdef blocks against build flags: {sorted(DEFINED_FLAGS) or '(none)'}
 *
 * Configuration folder: {rel_folder}
 * ============================================================================
 */

#pragma once

#include <stdint.h>

namespace combus {{
namespace wire {{

static constexpr uint8_t kCombusWireMd5[16] = {{
    {md5_bytes}
}};

static constexpr uint8_t kProjectVersionMajor = {VER_MAJOR}u;
static constexpr uint8_t kProjectVersionMinor = {VER_MINOR}u;

static constexpr uint8_t kCombusHandshakeWirePayloadLen = 18u;

}} // namespace wire
}} // namespace combus
"""

    out_path = folder / "combus_ids_remote_md5.h"
    out_path.write_text(header, encoding="utf-8")
    log(
        f"{rel_folder} — MD5={md5_hex}, ver={VER_MAJOR}.{VER_MINOR}, "
        f"flags={sorted(DEFINED_FLAGS) or 'none'} -> "
        f"{out_path.relative_to(PROJECT_ROOT).as_posix()}"
    )


# =============================================================================
# MAIN
# =============================================================================

for folder, inc_files in PAIRS:
    emit_md5_header(folder, inc_files)
