#!/usr/bin/env python3
"""
combus_builder/md5.py - A7.2: MD5 computation for the three combus views.

Generates a deterministic 16-byte MD5 hash for each of the three views:
  - combus_remote : REMOTE only (what crosses the wire)
  - combus_local  : REMOTE + LOCAL (inter-node alignment)
  - combus        : REMOTE + LOCAL + SYSTEM (full picture)

Inputs (identical for all three, per A7.1):
  - id, type, scope, theme, direction (sorted list), infoName
  - Channels in canonical order: canon `sort_key()` (scope LOCAL<REMOTE<SYSTEM,
    type, theme, id). NOT the key of _select_view() in generator.py, which
    numbers the IDs with REMOTE<LOCAL<SYSTEM.

Independence:
  - file discovery order (A3 stable output -> A5 stable order)
  - YAML formatting, comments
  - key ordering within channel dicts (JSON sort_keys=True)
  - requires (its effect is already captured by the channel's
    presence/absence in the view after A6.1 resolution)

projectVersion is NOT hashed — emitted as a SHARED constant
(kProjectVersionMajor, kProjectVersionMinor, kCombusHandshakeWirePayloadLen)
so a mismatch produces a precise diagnostic (option B).

Architecture (A7.2):
  - 3 view-specific headers:
      combus_remote_md5.h  ->  kCombusRemoteComBusMd5[16] + Hex
      combus_local_md5.h   ->  kCombusLocalComBusMd5[16]  + Hex
      combus_md5.h         ->  kCombusComBusMd5[16]       + Hex
  - 1 shared header:
      combus_wire_common.h ->  kProjectVersionMajor/Minor,
                               kCombusHandshakeWirePayloadLen
    Included by each view-specific header.
  - Each view header declares ONLY its own md5/hex constants. The shared
    header declares the version/payload-len constants ONCE. This avoids
    redefinition when all four headers are included in the same TU.
  - C++ uses `static constexpr` (xtensa-esp32-elf-g++ rejects inline
    constexpr at namespace scope in some configurations).

A7.2 history:
  - A7: 2 hashes (combus_local + combus_remote), with kMachineType.
  - A7.1: simplification to combus_remote only + kMachineType removed.
  - A7.2: restore the 3 views (combus + combus_local + combus_remote).
    kMachineType stays removed. The shared constants pattern replaces
    the A7.1 'emit shared only in combus_local_md5.h' hack.
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass

from .generator import View, ViewChannel


# =============================================================================
# CANONICAL SERIALIZATION
# =============================================================================

def _canonical_direction_for_hash(
    direction: frozenset[str],
    direction_or: frozenset[str] = frozenset(),
) -> list[str]:
    """Canonise `direction` + `direction_or` for MD5 hashing (ND7 — ONE_WAY, A2.1).

    Rationale (ND7):
      In a multi-board node, the same LOCAL channel can legitimately
      appear as `uplink` on one board and `downlink` on another — this
      is a role mirror, not a contract drift. The MD5 must NOT flag
      this as a divergence between boards.

      `both`, `both_or` and `none` are real contract signals (the
      channel is genuinely bidirectional, anti-oscillation OR, or
      genuinely absent on the wire) and must remain distinguishable
      from each other and from the one-way cases.

    A2.1 mapping (direction + direction_or → canonical list):
      - uplink        → ['ONE_WAY']
      - downlink      → ['ONE_WAY']
      - uplink_or     → ['ONE_WAY']   (same family as uplink)
      - downlink_or   → ['ONE_WAY']   (same family as downlink)
      - both          → ['downlink','uplink']
      - both_or       → ['both_or','downlink','uplink']  (distinct from both)
      - none          → []

    This function is HASH-ONLY. It must NEVER be used for code
    generation — the generated C++ must preserve the real direction
    so that the runtime can dispatch on uplink/downlink/both/none.
    """
    # A2.1 : direction and direction_or are mutually exclusive.
    # The effective direction is direction | direction_or.
    if direction_or:
        # *OR token — direction is empty, direction_or carries the wire sense.
        if direction_or == frozenset({"uplink", "downlink"}):
            # both_or — distinct from both (anti-oscillation flag matters)
            return ["both_or", "downlink", "uplink"]
        # uplink_or or downlink_or — one-way, neutralise uplink/downlink
        return ["ONE_WAY"]
    # No OR — plain direction.
    if direction == frozenset({"uplink", "downlink"}):
        # both — keep sorted as-is
        return sorted(direction)
    if direction == frozenset({"uplink"}) or direction == frozenset({"downlink"}):
        # one-way — neutralise the uplink/downlink distinction
        return ["ONE_WAY"]
    # none (or any other unexpected value) — keep as-is
    return sorted(direction)


def _view_channel_canonical(vc: ViewChannel, apply_one_way: bool = False) -> dict:
    """Return a JSON-serialisable dict representing one channel.

    The dict keys are a fixed list (no per-channel metadata). The direction
    is canonised via `_canonical_direction_for_hash()` (ND7 ONE_WAY) ONLY
    when `apply_one_way` is True — i.e. for the LOCAL view. For REMOTE and
    full views, the raw direction is kept (sorted) so that real contract
    signals (uplink vs downlink on REMOTE channels) are preserved.

    `infoName` is included so that renaming a debug label changes the hash.

    A2.1 (2c-bis) : `direction_or` is emitted as a separate field so
    that two channels identical except on the anti-oscillation flag
    produce different hashes. This matters for the LOCAL view: `both`
    and `both_or` share the same frozenset direction
    (`{uplink, downlink}`) but are treated as a real config
    divergence by diffusion.py (warning, not don't-care). The hash
    must therefore distinguish them so that a drift between boards
    built separately is detected — otherwise the runtime wiring of
    `bothOr` (next chantier) would silently mask a config bug.

    ONE_WAY canonisation is unaffected: it only neutralises the
    uplink/downlink distinction, never the both_or flag (carried by
    `direction_or`).
    """
    if apply_one_way:
        direction = _canonical_direction_for_hash(vc.ch.direction, vc.ch.direction_or)
        # A2.1 (2c-bis) : for LOCAL, direction_or must also be
        # canonised so that uplink/downlink/uplink_or/downlink_or
        # all collapse to the same hash. The ONE_WAY canonisation
        # neutralises the uplink/downlink distinction AND the
        # OR/non-OR distinction (the OR policy is local to the
        # producer — it is not a contract signal between boards).
        # both_or stays distinct (anti-oscillation flag matters for
        # the contract).
        if vc.ch.direction_or == frozenset({"uplink", "downlink"}):
            # both_or — distinct from both (anti-oscillation flag matters)
            direction_or = ["both_or", "downlink", "uplink"]
        elif direction == ["ONE_WAY"]:
            # uplink/downlink/uplink_or/downlink_or all collapse here.
            # direction_or must be [] so that uplink_or and downlink
            # produce the same hash (handshake between a producer
            # uplink_or and its mirror board downlink must succeed).
            direction_or = []
        else:
            direction_or = []
    else:
        # Remote / full views: no ONE_WAY canonisation.
        # direction is the raw frozenset (sorted), direction_or is
        # emitted separately so both/both_or remain distinguishable.
        direction = sorted(vc.ch.direction)
        direction_or = sorted(vc.ch.direction_or)
    return {
        "id": vc.ch.id,
        "type": vc.ch.type,
        "scope": vc.ch.scope,
        "theme": vc.ch.theme,
        "direction": direction,
        "direction_or": direction_or,
        "infoName": vc.ch.info_name,
    }


def canonical_bytes(view: View) -> bytes:
    """Return the canonical byte representation of a view, suitable for hashing.

    ND7 ONE_WAY canonisation is applied ONLY to the LOCAL view (the role
    mirror between boards of the same node is a LOCAL concept). REMOTE and
    full views keep the raw direction so that real contract signals
    (uplink vs downlink on REMOTE channels) are preserved.
    """
    apply_one_way = (view.name == "combus_local")
    payload = {
        "view": view.name,
        "channels": [_view_channel_canonical(vc, apply_one_way) for vc in view.channels],
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
    """The result of hashing a view.

    view_name: which view was hashed (combus, combus_local, combus_remote).
    md5_hex:   32-char lowercase hex string (128 bits).
    digest:    raw 16-byte MD5 digest.
    """
    view_name: str
    md5_hex: str
    digest: bytes


def compute_view_hash(view: View) -> ViewHash:
    """Compute MD5(canonical channels JSON) for the given view."""
    payload = canonical_bytes(view)
    digest = hashlib.md5(payload).digest()
    return ViewHash(
        view_name=view.name,
        md5_hex=digest.hex(),
        digest=digest,
    )


# =============================================================================
# C++ RENDERING
# =============================================================================

_CPP_HEADER_PROLOGUE = """\
/******************************************************************************
 * GENERATED FILE - DO NOT EDIT.
 *
 * Generated by scripts/combus_builder/generator.py (A7.2).
 * View: <view_name>
 * Hash inputs: id, type, scope, theme, direction, infoName
 *              (independent of file order, YAML formatting, requires).
 *
 * MD5 does NOT include projectVersion — emitted as a shared constant
 * in combus_wire_common.h (option B: separate hash + version for
 * precise mismatch diagnostics).
 * See scripts/combus_builder/README.md §7.
 *
 * Regenerate on every PlatformIO build via the combus_builder extra_script.
 * Do not commit this file to the source tree.
 ******************************************************************************/
"""

_CPP_COMMON_PROLOGUE = """\
/******************************************************************************
 * GENERATED FILE - DO NOT EDIT.
 *
 * Generated by scripts/combus_builder/generator.py (A7.2).
 * Shared constants for all three view-specific MD5 headers.
 *
 * This header is included by each view-specific MD5 header (combus_md5.h,
 * combus_local_md5.h, combus_remote_md5.h). Do NOT include this header
 * from application code — go through the view-specific header that
 * matches your view.
 *
 * Regenerate on every PlatformIO build via the combus_builder extra_script.
 * Do not commit this file to the source tree.
 ******************************************************************************/
"""

_PROJECT_VERSION_MAJOR_PLACEHOLDER = 0
_PROJECT_VERSION_MINOR_PLACEHOLDER = 1


# Mapping view_name -> (md5 symbol name, hex symbol name).
# The view_name is also used to capitalise the file name.
_VIEW_SYMBOLS = {
    "combus":        ("kCombusComBusMd5",        "kCombusComBusMd5Hex"),
    "combus_local":  ("kCombusLocalComBusMd5",   "kCombusLocalComBusMd5Hex"),
    "combus_remote": ("kCombusRemoteComBusMd5",  "kCombusRemoteComBusMd5Hex"),
}


def _render_common_header(
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
) -> str:
    """Render the shared combus_wire_common.h file.

    Contains the version + payload-len constants, declared ONCE so
    multiple view-specific headers can include this without redefinition.
    """
    body: list[str] = []
    body.append(_CPP_COMMON_PROLOGUE)
    body.append("")
    body.append("#pragma once")
    body.append("")
    body.append("#include <stdint.h>")
    body.append("")
    body.append("namespace combus {")
    body.append("namespace wire {")
    body.append("")
    body.append("/// Project version (placeholder 0.1 until project_version.h is introduced).")
    body.append(f"static constexpr uint8_t kProjectVersionMajor = {project_version_major}u;")
    body.append(f"static constexpr uint8_t kProjectVersionMinor = {project_version_minor}u;")
    body.append("")
    body.append("/// Total handshake wire payload length = 16 (md5) + 2 (version).")
    body.append("static constexpr uint8_t kCombusHandshakeWirePayloadLen = 18u;")
    body.append("")
    body.append("} // namespace wire")
    body.append("} // namespace combus")
    body.append("")
    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_view_header(
    hash_: ViewHash,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
) -> str:
    """Render a view-specific _md5.h file.

    Includes the shared header for projectVersion constants.
    Declares ONLY this view's md5 + hex constants.
    """
    view = hash_.view_name
    if view not in _VIEW_SYMBOLS:
        raise ValueError(f"unknown view: {view!r}")
    md5_sym, hex_sym = _VIEW_SYMBOLS[view]

    digest = hash_.digest
    hex_str = hash_.md5_hex

    prologue = _CPP_HEADER_PROLOGUE.replace("<view_name>", view)

    body: list[str] = []
    body.append(prologue)
    body.append("")
    body.append("#pragma once")
    body.append("")
    body.append('#include "combus_wire_common.h"')
    body.append("")
    body.append("#include <stdint.h>")
    body.append("")
    body.append("namespace combus {")
    body.append("namespace wire {")
    body.append("")
    body.append(f"/// MD5 hash of the canonical channel set for the {view} view.")
    body.append("/// 16 raw bytes (suitable for wire embedding).")
    body.append(f"static constexpr uint8_t {md5_sym}[16] = {{")
    body.append("    " + ", ".join(f"0x{b:02X}u" for b in digest))
    body.append("};")
    body.append("")
    body.append(f"/// Same MD5 as a 32-char lowercase hex string (debug/log).")
    body.append(f"static constexpr const char* {hex_sym} =")
    body.append(f'    "{hex_str}";')
    body.append("")
    body.append("} // namespace wire")
    body.append("} // namespace combus")
    body.append("")
    body.append("// EOF")
    return "\n".join(body) + "\n"


def emit_view_header(
    hash_: ViewHash,
    out_dir,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
) -> "Path":
    """Render and write the <view>_md5.h file for the given hash.

    Returns the path of the written file.
    """
    from pathlib import Path
    from .generator import _safe_write

    content = _render_view_header(
        hash_,
        project_version_major=project_version_major,
        project_version_minor=project_version_minor,
    )
    out_path = Path(out_dir) / f"{hash_.view_name}_md5.h"
    _safe_write(out_path, content)
    return out_path


def emit_common_header(
    out_dir,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
) -> "Path":
    """Render and write the shared combus_wire_common.h file.

    Returns the path of the written file.
    """
    from pathlib import Path
    from .generator import _safe_write

    content = _render_common_header(
        project_version_major=project_version_major,
        project_version_minor=project_version_minor,
    )
    out_path = Path(out_dir) / "combus_wire_common.h"
    _safe_write(out_path, content)
    return out_path


# =============================================================================
# PUBLIC ENTRY POINT
# =============================================================================

def generate_md5_artifacts(
    sel,
    out_dir,
    project_version_major: int = _PROJECT_VERSION_MAJOR_PLACEHOLDER,
    project_version_minor: int = _PROJECT_VERSION_MINOR_PLACEHOLDER,
):
    """Compute MD5 for the three combus views and write 4 headers into out_dir.

    Files emitted:
      - combus_remote_md5.h   (kCombusRemoteComBusMd5[16] + Hex)
      - combus_local_md5.h    (kCombusLocalComBusMd5[16]  + Hex)
      - combus_md5.h          (kCombusComBusMd5[16]       + Hex)
      - combus_wire_common.h  (kProjectVersionMajor/Minor,
                               kCombusHandshakeWirePayloadLen)

    Returns: dict[view_name -> ViewHash], list[Path]
    """
    hashes = {
        "combus_remote": compute_view_hash(sel.remote),
        "combus_local":  compute_view_hash(sel.local),
        "combus":        compute_view_hash(sel.full),
    }
    written = []
    # Always emit the shared header FIRST so a consumer that includes a
    # view-specific header after the shared header picks up the constants
    # in a stable order.
    written.append(emit_common_header(
        out_dir,
        project_version_major=project_version_major,
        project_version_minor=project_version_minor,
    ))
    for view_name, hash_ in hashes.items():
        written.append(emit_view_header(
            hash_,
            out_dir,
            project_version_major=project_version_major,
            project_version_minor=project_version_minor,
        ))
    return hashes, written
