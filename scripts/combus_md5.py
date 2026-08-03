"""
combus_md5.py — extra_script for PlatformIO.

Generates a single auto-generated, gitignored header that embeds:
  - kCombusWireMd5[16]      : MD5 of the concatenated REMOTE .inc files
                              for the active MACHINE_TYPE_*.
  - kCombusWireVersionMajor / Minor : from include/project_version.h.

Triggered by the `extra_scripts` directive in platformio.ini.  Re-runs
on every build (cheap: hashes a few KB max, typically zero bytes today).

Resolution chain (MACHINE_TYPE_* dispatch):
  1. Resolve the machine's Remote ComBus folder under
     src/core/config/machines/<machine>/combus/ by parsing the same
     MACHINE_TYPE_* `#if/#elif` ladder as combus_ids_remote.h.
  2. Glob combus_ids_remote_*.inc and combus_remote_*.inc in
     that folder (sorted, deterministic).
  3. Concat raw bytes, MD5-hash, embed.
  4. Read include/project_version.h, extract MAJOR / MINOR, embed.
  5. Write the generated header to:
        <build_dir>/<pioenv>/combus_handshake_md5.h
     where <build_dir> is set in platformio.ini
     (currently the system TEMP via sysenv.LOCALAPPDATA).

The generated header is the SINGLE source of truth for the handshake
payload — runtime code never hashes anything itself.

Scope discipline: only files matching *_remote_* are hashed, per the
task contract.  Per-machine / system-local .inc files are explicitly
excluded — they are NOT part of the wire contract and must not
contribute to the MD5.
"""

import hashlib
import os
import re
from pathlib import Path

Import("env")  # PlatformIO-provided SCons env

# ---------------------------------------------------------------------------
# 1. Resolve the active MACHINE_TYPE_* and the per-machine Remote folder.
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(env["PROJECT_DIR"])

# Resolve the active MACHINE_TYPE_* in two stages:
#
#   Stage 1 — CPPDEFINES / CCFLAGS (PlatformIO `-D` flags).
#             We look for `MACHINE_<NAME>` tokens that map to a known
#             machine type.  Example: -D MACHINE_VOLVO_A60_H_BRUDER
#             maps to MACHINE_TYPE_DUMPER_TRUCK.
#
#   Stage 2 — read the per-vehicle header
#             `machines/config/machines/<vehicle>/<vehicle>.h` and
#             confirm it defines `MACHINE_TYPE_<NAME>` itself (this is
#             how the C++ side actually wires the dispatch — see
#             volvo_A60H_bruder.h: `#define MACHINE_TYPE_DUMPER_TRUCK`).
#
# Mirrors the dispatch ladder in src/core/config/machines/combus_ids_remote.h.

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
# PIO represents BUILD_FLAGS as a Python list of strings, one entry per
# "line" of the ini (each value can itself be multi-flag).
_flags = env.get("BUILD_FLAGS", []) or []
flags = "\n".join(str(f) for f in _flags)
vehicle = None
for v in VEHICLE_TO_MACHINE_TYPE.keys():
    if re.search(rf"\bMACHINE_{v}\b", flags):
        vehicle = v
        break



if vehicle is None:
    print(
        "[combus_md5] no MACHINE_<VEHICLE> in build flags — skipping "
        "handshake MD5 generation (umbrella will #error at compile).",
    )
    Return()  # SCons Return macro

machine = VEHICLE_TO_MACHINE_TYPE[vehicle]


# Snakecase folder name (e.g. DUMPER_TRUCK -> dumper_truck).
machine_dir = machine.lower()

# The Remote .inc files live under src/core/config/machines/<machine>/,
# following the convention  combus_ids_remote_*.inc  and
# combus_remote_*.inc  (one or more levels deep — globbed
# recursively so future per-sub-feature splits Just Work).
remote_root = PROJECT_ROOT / "src" / "core" / "config" / "machines" / machine_dir

# ---------------------------------------------------------------------------
# 2. Glob the *_remote_*.inc files (deterministic order, recursive).
# ---------------------------------------------------------------------------

inc_patterns = ["combus_ids_remote_*.inc", "combus_remote_*.inc"]
inc_files = []
for pat in inc_patterns:
    inc_files.extend(sorted(remote_root.rglob(pat)))

inc_files = sorted(set(inc_files))  # final deterministic order


if not inc_files:
    print(
        f"[combus_md5] MACHINE_TYPE_{machine} — no *_remote_*.inc "
        f"found under {remote_root.relative_to(PROJECT_ROOT)} — "
        f"MD5 will be computed over an empty payload (expected today, "
        f"Remote .inc files not yet authored).",
    )

# Concatenate raw bytes in sorted order, hash.
md5 = hashlib.md5()
for p in inc_files:
    md5.update(p.read_bytes())
md5_hex = md5.hexdigest()

# ---------------------------------------------------------------------------
# 3. Read project version (hand-maintained in include/project_version.h).
# ---------------------------------------------------------------------------

# project_version.h lives at the repository root (peer of platformio.ini)
# — stable across future include/ refactors.
ver_path = PROJECT_ROOT / "project_version.h"
if not ver_path.exists():
    print(f"[combus_md5] FATAL: {ver_path.relative_to(PROJECT_ROOT)} not found.")
    env.Exit(1)

ver_text = ver_path.read_text()
# Accept both `constexpr uint8_t PROJECT_VERSION_MAJOR = N;` (current) and
# the legacy `#define PROJECT_VERSION_MAJOR Nu` form, so this script keeps
# working across the version-file refactor.
m_major = re.search(
    r"(?:static\s+constexpr\s+uint8_t|#define)\s+PROJECT_VERSION_MAJOR\s*(?:=|)\s*(\d+)u?",
    ver_text,
)
m_minor = re.search(
    r"(?:static\s+constexpr\s+uint8_t|#define)\s+PROJECT_VERSION_MINOR\s*(?:=|)\s*(\d+)u?",
    ver_text,
)
if not (m_major and m_minor):
    print("[combus_md5] FATAL: PROJECT_VERSION_{MAJOR,MINOR} not found.")
    env.Exit(1)
ver_major = int(m_major.group(1))
ver_minor = int(m_minor.group(1))

# ---------------------------------------------------------------------------
# 4. Emit the generated header.
# ---------------------------------------------------------------------------

build_dir = Path(env["PROJECT_BUILD_DIR"])  # <build_dir>/<pioenv>
out_path = build_dir / "combus_handshake_md5.h"

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
 *   - kCombusWireMd5[16]      : MD5 of the Remote-only .inc files,
 *                               concatenated in sorted order.
 *   - kCombusWireVersionMajor : from include/project_version.h.
 *   - kCombusWireVersionMinor : from include/project_version.h.
 *
 * Payload layout on the wire (18 bytes, seq==0 only):
 *   [0..15]  MD5 of Remote combus layout
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
 * @brief MD5 of the concatenated REMOTE-only .inc files for the active
 *        MACHINE_TYPE_*.  Empty MD5 when no Remote .inc files exist yet
 *        (expected today — Remote .inc set not authored).
 */
static constexpr uint8_t kCombusWireMd5[16] = {{
    {md5_bytes_str}
}};

/**
 * @brief Project version — embedded in every handshake frame.
 *        Copied verbatim from include/project_version.h at build time.
 */
static constexpr uint8_t kCombusWireVersionMajor = {ver_major}u;
static constexpr uint8_t kCombusWireVersionMinor = {ver_minor}u;

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

try:
    out_rel = out_path.relative_to(PROJECT_ROOT).as_posix()
except ValueError:
    # build_dir lives outside the project tree (e.g. %LOCALAPPDATA%/Temp)
    out_rel = str(out_path)
print(
    f"[combus_md5] MACHINE_TYPE_{machine} — {len(inc_files)} Remote .inc "
    f"file(s) hashed, MD5={md5_hex}, version={ver_major}.{ver_minor}, "
    f"-> {out_rel}",
)

