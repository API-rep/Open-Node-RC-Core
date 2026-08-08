"""
combus_md5.py — extra_script for PlatformIO.

Recursively scans src/core/ for any folder containing the pair
  combus_ids_remote_analog.inc
  combus_ids_remote_digital.inc
and emits, next to each pair, a generated header:
  combus_ids_remote_md5.h

Each generated header carries a single MD5 computed from:
  - PROJECT_VERSION_MAJOR / PROJECT_VERSION_MINOR (read from project_version.h)
  - content of the two .inc files (sorted by filename for determinism)

Scope is intentionally narrow:
  - No MACHINE_* / MACHINE_TYPE_* awareness.
  - No dispatcher dict.
  - No parsing of <machine>_config.h macros.
  - No umbrella aggregation file.

The script discovers configurations BY FILE PRESENCE, not by build context.
A new machine type only needs to drop its two .inc files under
src/core/config/machines/<type>/combus/ to be picked up on the next build.

Triggered by `extra_scripts = pre:scripts/combus_md5.py` in [env].  Runs
once per build.  Hashing cost is negligible (a few KB max).

Generated files are gitignored — see .gitignore (`combus_ids_remote_md5.h`).
"""

import hashlib
import os
import re
import sys
from pathlib import Path


Import("env")  # PlatformIO-provided SCons env

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(env["PROJECT_DIR"])
SRC_ROOT     = PROJECT_ROOT / "src"
SCAN_ROOT    = SRC_ROOT / "core"   # scope = src/core/ only (per task spec)


# ---------------------------------------------------------------------------
# Safe ASCII logger (PlatformIO's stdout is ascii-only — see old script
# for the full rationale; paths under PROJECT_ROOT stay ASCII).
# ---------------------------------------------------------------------------

def _safe(*args):
    out = []
    for a in args:
        if isinstance(a, Path):
            try:
                a = a.relative_to(PROJECT_ROOT).as_posix()
            except ValueError:
                a = a.as_posix()
        out.append(str(a).encode("ascii", errors="replace").decode("ascii"))
    print(*out)


# ---------------------------------------------------------------------------
# 1. Read project_version.h — strict regex for MAJOR / MINOR.
# ---------------------------------------------------------------------------

ver_path = PROJECT_ROOT / "project_version.h"
if not ver_path.exists():
    _safe(f"[combus_md5] FATAL: {ver_path} not found.")
    env.Exit(1)

ver_text = ver_path.read_text(encoding="utf-8")
m_major = re.search(
    r"(?:static\s+constexpr\s+uint8_t|#define)\s+PROJECT_VERSION_MAJOR\s*(?:=|)\s*(\d+)u?",
    ver_text,
)
m_minor = re.search(
    r"(?:static\s+constexpr\s+uint8_t|#define)\s+PROJECT_VERSION_MINOR\s*(?:=|)\s*(\d+)u?",
    ver_text,
)
if not (m_major and m_minor):
    _safe("[combus_md5] FATAL: PROJECT_VERSION_{MAJOR,MINOR} not found in "
          f"{ver_path}.")
    env.Exit(1)

ver_major = int(m_major.group(1))
ver_minor = int(m_minor.group(1))


# ---------------------------------------------------------------------------
# 2. Recursively scan src/core/ for pairs of REMOTE-ID .inc files.
# ---------------------------------------------------------------------------

PAIR_ANALOG = "combus_ids_remote_analog.inc"
PAIR_DIGITL = "combus_ids_remote_digital.inc"


def _find_pairs(root: Path) -> list:
    """Return list of (folder, [analog_path, digital_path]) tuples for every
    folder under `root` containing both REMOTE-ID .inc files.

    A folder with only one of the two is ignored (not a complete config).
    """
    pairs = []
    for dirpath, _dirnames, filenames in os.walk(root):
        if PAIR_ANALOG in filenames and PAIR_DIGITL in filenames:
            folder = Path(dirpath)
            pairs.append((
                folder,
                [folder / PAIR_ANALOG, folder / PAIR_DIGITL],
            ))
    # Deterministic order: sort by folder path (POSIX, relative to root).
    pairs.sort(key=lambda kv: kv[0].as_posix())
    return pairs


pairs = _find_pairs(SCAN_ROOT)
if not pairs:
    _safe(f"[combus_md5] no combus_ids_remote_{{analog,digital}}.inc pair "
          f"found under {SCAN_ROOT.as_posix()} — nothing to generate.")


# ---------------------------------------------------------------------------
# 3. Emit one combus_ids_remote_md5.h per pair.
# ---------------------------------------------------------------------------

def _hash_input(ver_major: int, ver_minor: int, inc_files: list) -> bytes:
    """Build the canonical byte string to MD5-hash.

    Layout (deterministic, identical to the previous semantics):
      1. Version header : "v<major>.<minor>\n" (ASCII decimal, no padding).
      2. Per-file       : <path-as-posix>\n then <content bytes>\n.
         Files are processed in sorted order.
    """
    parts = [f"v{ver_major}.{ver_minor}\n".encode("ascii")]
    for p in sorted(inc_files):  # sorted = stable order
        # Use relative-to-PROJECT_ROOT path so non-ASCII chars in the
        # absolute path (e.g. "Mod\u00e8lisme") never reach the ASCII encoder.
        rel = p.relative_to(PROJECT_ROOT).as_posix()
        parts.append(rel.encode("ascii"))
        parts.append(b"\n")
        parts.append(p.read_bytes())
        parts.append(b"\n")
    return b"".join(parts)


for folder, inc_files in pairs:
    hash_in = _hash_input(ver_major, ver_minor, inc_files)
    md5_hex = hashlib.md5(hash_in).hexdigest()
    md5_bytes_str = ", ".join(f"0x{b:02X}u" for b in bytes.fromhex(md5_hex))

    header = f"""\
/* ============================================================================
 * combus_ids_remote_md5.h  —  AUTO-GENERATED by scripts/combus_md5.py
 *
 * DO NOT EDIT.  Re-generated on every PlatformIO build.  Gitignored.
 *
 * Carries the MD5 of the canonical byte string built from:
 *   - PROJECT_VERSION_MAJOR / PROJECT_VERSION_MINOR (from project_version.h)
 *   - content of combus_ids_remote_analog.inc
 *   - content of combus_ids_remote_digital.inc
 *
 * Scope of this header: the configuration located in:
 *   {folder.relative_to(PROJECT_ROOT).as_posix()}
 *
 * Discovered (and re-hashed) on every build.  No build-flag awareness —
 * the file pair itself is the discovery anchor.
 * ============================================================================
 */

#pragma once

#include <stdint.h>

namespace combus {{
namespace wire {{

/**
 * @brief MD5 of (version header + REMOTE-ID .inc contents) for the
 *        configuration located at
 *        {folder.relative_to(PROJECT_ROOT).as_posix()}.
 *
 *        Hash input layout (deterministic — see scripts/combus_md5.py):
 *          - "v<major>.<minor>\\n"
 *          - for each .inc (sorted by filename):
 *              "<path-as-posix>\\n" + <content> + "\\n"
 */
static constexpr uint8_t kCombusWireMd5[16] = {{
    {md5_bytes_str}
}};

/**
 * @brief Project version embedded in the hash for this configuration.
 *        Copied verbatim from project_version.h at build time.
 */
static constexpr uint8_t kProjectVersionMajor = {ver_major}u;
static constexpr uint8_t kProjectVersionMinor = {ver_minor}u;

/**
 * @brief Wire payload length (bytes) of one handshake frame.
 *        = 16 (md5) + 1 (major) + 1 (minor) = 18.
 *
 *        This is the SOLE wire contract for the handshake stub.  Any
 *        change here MUST match combus_handshake.h's
 *        `kCombusHandshakePayloadLen` (the umbrella enforces the match
 *        with a `static_assert`).
 */
static constexpr uint8_t kCombusHandshakeWirePayloadLen = 18u;

}}  // namespace wire
}}  // namespace combus
"""


    out_path = folder / "combus_ids_remote_md5.h"
    out_path.write_text(header, encoding="utf-8")

    _safe(
        f"[combus_md5] {folder.relative_to(PROJECT_ROOT).as_posix()} — "
        f"MD5={md5_hex}, version={ver_major}.{ver_minor} "
        f"-> {out_path.relative_to(PROJECT_ROOT).as_posix()}",
    )


# No CPPPATH append: generated headers live under src/core/ which is
# already reachable via the project-wide `-I src` build flag.
