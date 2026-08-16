#!/usr/bin/env python3
"""
combus_builder/md5.py - A7: MD5 computation for combus views.

Generates a deterministic 16-byte MD5 hash of a view's channel definitions
(id, type, scope, theme, direction, infoName). The hash is independent of:
  - file discovery order
  - YAML formatting, comments
  - key ordering within channel dicts
  - `requires` (its effect is already captured by the channel's presence/absence
    in the view after A8 resolution)

The hash does NOT cover machineType or projectVersion — those are exposed
as separate constants so the handshake can produce a precise diagnostic
on mismatch (option B in the A7 prompt).

Pre-A7 audit observations (see doc/combus_v2 - YAML implementation.md §13):
  - The stale file `src/core/config/machines/dumper_truck/combus/
    combus_ids_remote_md5.h` (auto-generated, not committed in the working
    tree's source-of-truth) defined `kCombusWireMd5[16]` + separate
    `kProjectVersionMajor/Minor` + `kCombusHandshakeWirePayloadLen = 18u`.
    The 18 = 16 (md5) + 2 (version) layout confirms option B was the
    intended shape.
  - No `combus_handshake.h/.cpp` exists in the working tree yet. The
    handshake consumer side is out of scope for A7; this module only
    produces the hash and the version constants.
  - No `project_version.h` exists. Version is a placeholder (0/1) until a
    project-level version source is introduced (out of A7 scope).
  - `machineType` is a CPP flag (`MACHINE_TYPE_DUMPER_TRUCK`, etc.)
    selected by `extends` in platformio.ini — not a numeric value. It
    cannot be hashed directly. The BuildContext exposes it through the
    `MACHINE_TYPE_*` flag, which the generator passes to this module
    as an opaque string token.

Canonical representation:
  - JSON with sort_keys=True over a fixed list of fields per channel.
  - Channels ordered by (scope priority, type, theme, id) — the same
    canonical order used by _select_view() in generator.py.
  - Field per channel: id, type, scope, theme, direction (sorted list),
    infoName.

C++ emission policy (A7.1 — fixed xtensa-esp32-elf-g++ incompatibility):
  - Both files use `static constexpr` (linkage-safe pattern that works on
    every supported toolchain).
  - Shared constants (kProjectVersionMajor/Minor, kCombusHandshakeWirePayloadLen,
    kMachineType) are emitted in combus_local_md5.h ONLY. combus_remote_md5.h
    does NOT redefine them.
  - The contract for consumers: include combus_local_md5.h FIRST if you
    need access to the shared constants; combus_remote_md5.h is standalone
    if you only need the per-view MD5.
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Iterable

from .generator import View, ViewChannel


# =============================================================================
# CANONICAL SERIALIZATION
# =============================================================================

def _view_channel_canonical(vc: ViewChannel) -> dict:
    """
    Return a JSON-serialisable dict representing one channel.

    The dict keys are a fixed list (no per-channel metadata). The direction
    is sorted to remove any ordering ambiguity. `infoName` is included so
    that renaming a debug label changes the hash.
    """
    direction = sorted(vc.ch.direction)
    return {
        "id": vc.ch.id,
        "type": vc.ch.type,
        "scope": vc.ch.scope,
        "theme": vc.ch.theme,
        "direction": direction,
        "infoName": vc.ch.info_name,
    }


def canonical_bytes(view: View) -> bytes:
    """
    Return the canonical byte representation of a view, suitable for hashing.

    Format: UTF-8 JSON, sort_keys=True, with a top-level marker that
    distinguishes combus_local vs combus_remote. Without this marker the
    two views would produce distinct hashes anyway (different scopes),
    but the explicit label makes the intent obvious in a hex dump.
    """
    payload = {
        "view": view.name,
        "channels": [_view_channel_canonical(vc) for vc in view.channels],
    }
    return json.dumps(
        payload,
        sort_keys=True,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")


# =============================================================================
# HASH COMPUTATION
# =============================================================================

@dataclass(frozen=True)
class ViewHash:
    """
    The result of hashing a view.

    view_name: which view was hashed (combus_local or combus_remote).
    md5_hex:   32-char lowercase hex string (128 bits).
    digest:    raw 16-byte MD5 digest.
    """
    view_name: str
    md5_hex: str
    digest: bytes


def compute_view_hash(view: View) -> ViewHash:
    """
    Compute MD5(view_name + canonical channels JSON).

    The view's channels are already in canonical order from _select_view,
    so no resort is needed here.
    """
    payload = canonical_bytes(view)
    digest = hashlib.md5(payload).digest()
    return ViewHash(
        view_name=view.name,
        md5_hex=digest.hex(),
        digest=digest,
    )


def compute_hashes(
    views: Iterable[View],
) -> dict[str, ViewHash]:
    """Compute ViewHash for each view in `views`, keyed by view.name."""
    return {v.name: compute_view_hash(v) for v in views}


# =============================================================================
# C++ RENDERING
# =============================================================================

_CPP_HEADER_PROLOGUE = """\
/******************************************************************************
 * GENERATED FILE - DO NOT EDIT.
 *
 * Generated by scripts/combus_builder/generator.py (A7).
 * View: <view_name>
 * Hash inputs: id, type, scope, theme, direction, infoName
 *              (independent of file order, YAML formatting, requires).
 * MachineType: <machine_type>
 * ProjectVersion: <project_version>
 *
 * MD5 does NOT include machineType or projectVersion — these are emitted
 * as separate constants below (see doc/combus_v2 - YAML implementation.md
 * §13, option B: separate hash + version for precise mismatch diagnostics).
 *
 * Regenerate on every PlatformIO build via the combus_builder extra_script.
 * Do not commit this file to the source tree.
 ******************************************************************************/
"""

_PROJECT_VERSION_MAJOR_PLACEHOLDER = 0
_PROJECT_VERSION_MINOR_PLACEHOLDER = 1
_MACHINE_TYPE_PLACEHOLDER = "UNCONFIGURED"


def _render_md5_header(
    view_name: str,
    hash_: ViewHash,
    machine_type: str = _MACHINE_TYPE_PLACEHOLDER,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
) -> str:
    """
    Render the `<view>_md5.h` file.

    Per-view emitted (every file):
      - k<Cap>ComBusMd5[16]            raw MD5 bytes (suitable for wire)
      - k<Cap>ComBusMd5Hex             32-char lowercase hex (debug/log)

    Shared constants emitted ONLY in combus_local_md5.h:
      - kProjectVersionMajor / Minor   placeholder until project_version.h
      - kCombusHandshakeWirePayloadLen 18 (= 16 md5 + 2 version)
      - kMachineType                   string token (log/debug)

    Note: machineType and projectVersion are emitted as separate constants
    so the handshake consumer can produce a precise diagnostic on mismatch
    (option B). They are NOT part of the MD5 input.
    """
    cap = view_name.capitalize()
    digest = hash_.digest
    hex_str = hash_.md5_hex

    body: list[str] = []
    body.append(_CPP_HEADER_PROLOGUE
                .replace("<view_name>", view_name)
                .replace("<machine_type>", machine_type)
                .replace("<project_version>", f"{project_version_major}.{project_version_minor}"))
    body.append("")
    body.append("#pragma once")
    body.append("")
    body.append("#include <stdint.h>")
    body.append("")
    body.append("namespace combus {")
    body.append("namespace wire {")
    body.append("")
    body.append(f"/// MD5 hash of the canonical channel set for the {view_name} view.")
    body.append(f"/// 16 raw bytes (suitable for wire embedding).")
    body.append(f"static constexpr uint8_t k{cap}ComBusMd5[16] = {{")
    body.append("    " + ", ".join(f"0x{b:02X}u" for b in digest))
    body.append("};")
    body.append("")
    body.append(f"/// Same MD5 as a 32-char lowercase hex string (debug/log).")
    body.append(f"static constexpr const char* k{cap}ComBusMd5Hex =")
    body.append(f"    \"{hex_str}\";")
    body.append("")
    # Shared constants are emitted only in combus_local_md5.h to avoid
    # redefinition errors when both headers are included in the same TU.
    # Consumer contract: include combus_local_md5.h FIRST if you need the
    # shared constants; combus_remote_md5.h is standalone for the per-view
    # MD5 only.
    if view_name == "combus_local":
        body.append("/// Project version (placeholder 0.1 until project_version.h is introduced).")
        body.append(f"static constexpr uint8_t kProjectVersionMajor = {project_version_major}u;")
        body.append(f"static constexpr uint8_t kProjectVersionMinor = {project_version_minor}u;")
        body.append("")
        body.append("/// Total handshake wire payload length = 16 (md5) + 2 (version).")
        body.append("static constexpr uint8_t kCombusHandshakeWirePayloadLen = 18u;")
        body.append("")
        body.append("/// Machine type token (for log/debug).")
        body.append(f"static constexpr const char* kMachineType = \"{machine_type}\";")
    body.append("")
    body.append("} // namespace wire")
    body.append("} // namespace combus")
    body.append("")
    body.append("// EOF")
    return "\n".join(body) + "\n"


def emit_md5_header(
    view_name: str,
    hash_: ViewHash,
    out_dir,
    machine_type: str = _MACHINE_TYPE_PLACEHOLDER,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
) -> "Path":
    """Render and write the <view>_md5.h file into out_dir."""
    from pathlib import Path
    from .generator import _safe_write

    content = _render_md5_header(
        view_name,
        hash_,
        machine_type=machine_type,
        project_version_major=project_version_major,
        project_version_minor=project_version_minor,
    )
    out_path = Path(out_dir) / f"{view_name}_md5.h"
    _safe_write(out_path, content)
    return out_path


# =============================================================================
# PUBLIC ENTRY POINT
# =============================================================================

def generate_md5_artifacts(
    sel,
    out_dir,
    machine_type: str = _MACHINE_TYPE_PLACEHOLDER,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
):
    """
    Compute MD5 hashes for combus_local and combus_remote (per the A7 spec)
    and write their <view>_md5.h files into out_dir.

    combus (full) is intentionally NOT hashed: it differs legitimately
    between cards of the same node (different SYSTEM channels), so it
    is the wrong basis for an inter-node alignment check. combus_local
    (REMOTE+LOCAL) replaces it for that purpose.

    Returns: (hashes, written) where:
      - hashes  : dict[str, ViewHash] keyed by view.name
      - written : dict[str, Path] mapping view name -> written md5 header path
    """
    hashes = compute_hashes([sel.local, sel.remote])
    written = {}
    for view_name, h in hashes.items():
        written[view_name] = emit_md5_header(
            view_name,
            h,
            out_dir,
            machine_type=machine_type,
            project_version_major=project_version_major,
            project_version_minor=project_version_minor,
        )
    return hashes, written
