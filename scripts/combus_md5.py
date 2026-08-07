"""
combus_md5.py — extra_script for PlatformIO.

Generates a single auto-generated, gitignored header that embeds:
  - kCombusWireMd5[16]            : MD5 of the canonical byte string built
                                    from version + REMOTE .inc files for
                                    the active MACHINE_TYPE_*.
  - kProjectVersionMajor/Minor    : from project_version.h.
                                    Naming reflects PROJECT-level contract,
                                    not combus-specific (the same constants
                                    can be reused by other subsystems in the
                                    future).


Triggered by the `extra_scripts` directive in platformio.ini.  Re-runs
on every build (cheap: hashes a few KB max, typically zero bytes today).

Resolution chain (MACHINE_TYPE_* dispatch):
  1. Resolve the active MACHINE_TYPE_* by parsing the same `MACHINE_*`
     dispatch ladder in platformio.ini's BUILD_FLAGS.
  2. Read <machine_type>_config.h (e.g. dumper_truck_config.h) and parse
     the two macros COMBUS_IDS_REMOTE_ANALOG_INC and
     COMBUS_IDS_REMOTE_DIGITAL_INC — they hold the POSIX-style paths to
     the Remote .inc files for the active TYPE.

     TODO(post-rework layering combus):
       Once the combus layering refactor lands (multi-root -I overlay,
       no more macros), replace this step 2 with direct path resolution
       under <machine_type>/combus/. Keep the rest of this script stable.
  3. Build a canonical byte string to hash:
       - "v<major>.<minor>\n" header (so version bumps change the MD5).
       - Each .inc file path + content, in sorted order, newline-separated.
     Reformatting, comments or whitespace in project_version.h or .inc
     files have NO effect on the hash. Only:
       - bumping MAJOR/MINOR values
       - adding/removing/reordering tokens in .inc files
       - renaming an .inc file
     change the hash.
  4. MD5-hash the canonical byte string, embed.
  5. Read project_version.h, extract MAJOR / MINOR via strict regex,
     embed separately for dashboard / log traces.
  6. Write the generated header to:
        <build_dir>/<pioenv>/combus_handshake_md5.h

The generated header is the SINGLE source of truth for the handshake
payload — runtime code never hashes anything itself.

Scope discipline: only the *_IDS_REMOTE_*.inc macros are hashed, per the
task contract.  Per-machine / system-local .inc files are explicitly
excluded — they are NOT part of the wire contract and must not
contribute to the MD5.
"""

import hashlib
import os
import re
import sys
from pathlib import Path


Import("env")  # PlatformIO-provided SCons env

# ---------------------------------------------------------------------------
# 1. Resolve the active MACHINE_TYPE_*.
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(env["PROJECT_DIR"])
SRC_ROOT     = PROJECT_ROOT / "src"

# Helper for safe logging under PlatformIO's ascii stdout.
# The PlatformIO logger re-wraps sys.stdout with an ascii codec and ignores
# sys.stdout.reconfigure() / PYTHONIOENCODING.  Any non-ASCII char in a path
# (e.g. "Mod\u00e8lisme" in PROJECT_ROOT) crashes the builtin print.
# Workaround: always pass paths via .as_posix() — relative-to-PROJECT_ROOT
# paths have NO non-ASCII char (project root name stays outside).  For
# absolute paths we ASCII-escape them so the print never sees the raw byte.
def _safe(*args):
    """ASCII-safe formatter for log lines."""
    out = []
    for a in args:
        if isinstance(a, Path):
            try:
                a = a.relative_to(PROJECT_ROOT).as_posix()
            except ValueError:
                a = a.as_posix()
        out.append(str(a).encode("ascii", errors="replace").decode("ascii"))
    print(*out)






# Vehicle-define (CPP side)  ->  machine_type folder (MACHINE_TYPE_* side)
VEHICLE_TO_MACHINE_TYPE = {
    "VOLVO_A60_H_BRUDER": "DUMPER_TRUCK",
    # add future vehicles here, key = the `MACHINE_<X>` define, value =
    # the MACHINE_TYPE_* token the corresponding <vehicle>.h defines.
}

# At the pre-script stage (runs BEFORE the toolchain is initialised), the
# SCons env does not yet expose CPPDEFINES / CCFLAGS directly — but
# BUILD_FLAGS holds the raw, fully-expanded flags PlatformIO has merged
# from every env layer (env / env:parent / env:<this> / [volvo_A60H_id]).
_flags = env.get("BUILD_FLAGS", []) or []
flags = "\n".join(str(f) for f in _flags)
vehicle = None
for v in VEHICLE_TO_MACHINE_TYPE.keys():
    if re.search(rf"\bMACHINE_{v}\b", flags):
        vehicle = v
        break


if vehicle is None:
    _safe("[combus_md5] no MACHINE_<VEHICLE> in build flags — skipping "
          "handshake MD5 generation (umbrella will #error at compile).")
    Return()  # SCons Return macro


machine = VEHICLE_TO_MACHINE_TYPE[vehicle]
machine_dir = machine.lower()  # DUMPER_TRUCK -> dumper_truck


# ---------------------------------------------------------------------------
# 2. Resolve Remote .inc paths from the macros in <machine>_config.h.
# ---------------------------------------------------------------------------
#
# Today: COMBUS_IDS_REMOTE_ANALOG_INC / _DIGITAL_INC are #define'd in
#        src/core/config/machines/<machine>/<machine>_config.h to POSIX-style
#        paths relative to the src/ CPPPATH root (e.g. <core/.../foo.inc>).
#        We parse the #define lines directly — no preprocessor call.
#
# TODO(post-rework layering combus):
#   Once the layering refactor replaces macros with multi-root -I overlay,
#   swap _extract_inc_paths() for direct rglob under
#   src/core/machines/<machine>/combus/combus_ids_remote_*.inc.

def _extract_inc_paths(config_h: Path) -> list:
    """Return the filesystem paths referenced by COMBUS_IDS_REMOTE_*_INC.

    Parses `#define COMBUS_IDS_REMOTE_<K>_INC <path>` lines from
    <machine>_config.h.  Returns absolute Paths, sorted for determinism.
    """
    if not config_h.exists():
        _safe(f"[combus_md5] FATAL: {config_h} "
              f"not found — cannot resolve COMBUS_IDS_REMOTE_*_INC.")
        env.Exit(1)


    text = config_h.read_text(encoding="utf-8")
    macros = ("COMBUS_IDS_REMOTE_ANALOG_INC", "COMBUS_IDS_REMOTE_DIGITAL_INC")

    paths = []
    for name in macros:
        # Capture the right-hand side, strip surrounding <>/"" and whitespace.
        m = re.search(rf"^\s*#define\s+{name}\s+(.+?)\s*$", text, re.MULTILINE)
        if not m:
            _safe(f"[combus_md5] FATAL: macro {name} not found in "
                  f"{config_h}.")
            env.Exit(1)

        raw = m.group(1).strip().strip("<>").strip('"')
        # raw is a path relative to the src/ CPPPATH root.
        paths.append(SRC_ROOT / raw)

    return sorted(paths)


config_h = (SRC_ROOT / "core" / "config" / "machines" / machine_dir
            / f"{machine_dir}_config.h")
inc_files = _extract_inc_paths(config_h)


# Sanity-check that the resolved paths actually exist on disk.
for p in inc_files:
    if not p.exists():
        _safe(f"[combus_md5] FATAL: resolved .inc path does not exist: "
              f"{p}")
        env.Exit(1)



if not inc_files:
    _safe(
        f"[combus_md5] MACHINE_TYPE_{machine} — no *_IDS_REMOTE_*.inc "
        f"resolved from {config_h} — "
        f"MD5 will be computed over an empty payload.",
    )



# ---------------------------------------------------------------------------
# 3. Read project_version.h (strict regex, MAJOR + MINOR).
# ---------------------------------------------------------------------------

# project_version.h lives at the repository root (peer of platformio.ini)
# — stable across future include/ refactors.
ver_path = PROJECT_ROOT / "project_version.h"
if not ver_path.exists():
    _safe(f"[combus_md5] FATAL: {ver_path} not found.")
    env.Exit(1)


ver_text = ver_path.read_text(encoding="utf-8")
# Accept both `constexpr uint8_t PROJECT_VERSION_MAJOR = N;` (current) and
# the legacy `#define PROJECT_VERSION_MAJOR Nu` form.
#
# IMPORTANT: do NOT rename the constants PROJECT_VERSION_MAJOR / MINOR.
# See the DO NOT RENAME block at the top of project_version.h.
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
# 4. Build canonical byte string and hash it.
# ---------------------------------------------------------------------------

def _build_hash_input(ver_major: int, ver_minor: int,
                      inc_files: list) -> bytes:
    """Build the canonical byte string to MD5-hash.

    Order is fixed:
      1. Version header  : "v<major>.<minor>\n"  (ASCII decimal, no padding).
      2. Per-file        : <path-as-posix>\n then <content bytes>\n.
         Files are processed in sorted path order.

    Any reformatting of project_version.h or .inc files (comments,
    whitespace, alignment, trailing `u`) has NO effect. Only:
      - bumping MAJOR / MINOR values
      - adding / removing / reordering tokens in .inc files
      - renaming an .inc file
    change the hash.
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



hash_input = _build_hash_input(ver_major, ver_minor, inc_files)
md5_hex   = hashlib.md5(hash_input).hexdigest()


# ---------------------------------------------------------------------------
# 5. Emit the generated header.
# ---------------------------------------------------------------------------

build_dir = Path(env["PROJECT_BUILD_DIR"])  # <build_dir>/<pioenv>
out_path  = build_dir / "combus_handshake_md5.h"

# Make the generated header discoverable via the standard `<...>` include
# search path so combus_handshake.{h,cpp} can `#include "combus_handshake_md5.h"`
# without a path.  Path must be POSIX-style + trailing slash for CPPPATH.
env.Append(CPPPATH=[str(build_dir.as_posix()) + "/"])


md5_bytes_str = ", ".join(f"0x{b:02X}u" for b in bytes.fromhex(md5_hex))

inc_listing = " ".join(
    p.relative_to(PROJECT_ROOT).as_posix() for p in inc_files
) if inc_files else "(empty payload)"


header = f"""\
/* ============================================================================
 * combus_handshake_md5.h  —  AUTO-GENERATED by scripts/combus_md5.py
 *
 * DO NOT EDIT.  Re-generated on every PlatformIO build.  Gitignored
 * (see .gitignore:  /combus_handshake_md5.h and /scripts/__pycache__/).
 *
 * Single source of truth for the ComBus handshake payload wire bytes:
 *   - kCombusWireMd5[16]    : MD5 of the canonical byte string built
 *                             from version + REMOTE .inc files (sorted).
 *   - kProjectVersionMajor  : from project_version.h (project-level).
 *   - kProjectVersionMinor  : from project_version.h (project-level).

 *
 * Payload layout on the wire (18 bytes, seq==0 only):
 *   [0..15]  MD5 of (version header + REMOTE .inc contents)
 *   [16]     version major
 *   [17]     version minor
 *
 * ============================================================================
 */

#pragma once

#include <stdint.h>

namespace combus {{
namespace wire {{

/**
 * @brief MD5 of the canonical byte string built from version + REMOTE .inc
 *        files for the active MACHINE_TYPE_*.  Computed by
 *        scripts/combus_md5.py at build time.
 *
 *        Hash input layout (deterministic, see combus_md5.py for full spec):
 *          - "v<major>.<minor>\\n"
 *          - for each .inc (sorted by path):
 *              "<path-as-posix>\\n" + <content> + "\\n"
 *
 *        Empty payload when no Remote .inc files are resolved.
 */
static constexpr uint8_t kCombusWireMd5[16] = {{
    {md5_bytes_str}
}};

/**
 * @brief Project version — embedded in every handshake frame.
 *        Copied verbatim from project_version.h at build time.
 *
 *        Named "kProject*" (not "kCombusWire*") because these are the
 *        PROJECT-level contract version constants, reusable by any
 *        future subsystem (motion, sound, ...) that wants to advertise
 *        the same project contract identity.  They are NOT combus-specific.
 */
static constexpr uint8_t kProjectVersionMajor = {ver_major}u;
static constexpr uint8_t kProjectVersionMinor = {ver_minor}u;


/**
 * @brief Wire payload length (bytes) of one handshake frame.
 *        = 16 (md5) + 1 (major) + 1 (minor) = 18.
 */
static constexpr uint8_t kCombusHandshakeWirePayloadLen = 18u;

}}  // namespace wire
}}  // namespace combus

// ----------------------------------------------------------------------------
// Build provenance — useful for CI / dashboard traces.  Plain C string
// literal so it survives any future namespace refactor.
// ----------------------------------------------------------------------------
static constexpr char kCombusWireMd5Provenance[] =
    "md5 srcs (sorted): [{inc_listing}]";
"""

out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_text(header, encoding="utf-8")

_safe(
    f"[combus_md5] MACHINE_TYPE_{machine} — {len(inc_files)} Remote .inc "
    f"file(s) hashed, MD5={md5_hex}, version={ver_major}.{ver_minor}, "
    f"-> {out_path}",
)

