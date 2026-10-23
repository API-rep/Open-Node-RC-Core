#!/usr/bin/env python3
"""
combus_builder/md5.py - A7.2: MD5 computation for the three combus views.

Generates a deterministic 16-byte MD5 hash for each of the three views:
  - combus_base : BASE only (what crosses the wire)
  - combus_system  : BASE + SYSTEM (inter-node alignment)
  - combus        : BASE + SYSTEM + BOARD (full picture)

Inputs (identical for all three, chantier 2.1):
  - id, type, scope, direction (sorted list), value (rendered C++ literal)
  - Channels in wire order: the order of the view, i.e. the numeric IDs of
    _select_view() in generator.py (analog then digital; BASE<SYSTEM<BOARD,
    then theme, then id).

Independence:
  - file discovery order (A3 stable output -> A5 stable order)
  - YAML formatting, comments
  - key ordering within channel dicts (JSON sort_keys=True)
  - requires (its effect is already captured by the channel's
    presence/absence in the view after A6.1 resolution)
  - theme and infoName (chantier 2.1): a debug label, or a theme that does
    not move the channel on the wire, does not change the contract

projectVersion is NOT hashed — emitted as a SHARED constant
(kProjectVersionMajor, kProjectVersionMinor, kCombusHandshakeWirePayloadLen;
values read from project_version.h by read_project_version())
so a mismatch produces a precise diagnostic (option B).

Architecture (A7.2):
  - 3 view-specific headers:
      combus_base_md5.h  ->  kCombusBaseComBusMd5[16] + Hex
      combus_system_md5.h   ->  kCombusSystemComBusMd5[16]  + Hex
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
  - A7: 2 hashes (combus_system + combus_base), with kMachineType.
  - A7.1: simplification to combus_base only + kMachineType removed.
  - A7.2: restore the 3 views (combus + combus_system + combus_base).
    kMachineType stays removed. The shared constants pattern replaces
    the A7.1 'emit shared only in combus_system_md5.h' hack.
"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path

from .generator import View, ViewChannel, _render_value


# =============================================================================
# CANONICAL SERIALIZATION
# =============================================================================

def _canonical_direction_for_hash(
    direction: frozenset[str],
    direction_or: frozenset[str] = frozenset(),
) -> list[str]:
    """Canonise `direction` + `direction_or` for MD5 hashing (ND7 — ONE_WAY, A2.1).

    Rationale (ND7):
      In a multi-board node, the same SYSTEM channel can legitimately
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
    when `apply_one_way` is True — i.e. for the SYSTEM view. For BASE and
    full views, the raw direction is kept (sorted) so that real contract
    signals (uplink vs downlink on BASE channels) are preserved.

    `infoName` and `theme` are NOT included (chantier 2.1); `value` is.

    A2.1 (2c-bis) : `direction_or` is emitted as a separate field so
    that two channels identical except on the anti-oscillation flag
    produce different hashes. This matters for the SYSTEM view: `both`
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
        # A2.1 (2c-bis) : for SYSTEM, direction_or must also be
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
        # Base / full views: no ONE_WAY canonisation.
        # direction is the raw frozenset (sorted), direction_or is
        # emitted separately so both/both_or remain distinguishable.
        direction = sorted(vc.ch.direction)
        direction_or = sorted(vc.ch.direction_or)
    # Chantier 2.1 — only what makes the contract: the channel (id, type,
    # scope), its direction, and the default value it is sent with while its
    # writer has not written it (rendered C++ literal: an absent value and its
    # explicit default hash the same).  The position on the wire is the order
    # of the list.  theme and infoName are not hashed: a debug label or a theme
    # that does not move the channel does not change the wire.
    return {
        "id": vc.ch.id,
        "type": vc.ch.type,
        "scope": vc.ch.scope,
        "direction": direction,
        "direction_or": direction_or,
        "value": _render_value(vc.ch, vc.ch.type),
    }


def canonical_bytes(view: View) -> bytes:
    """Return the canonical byte representation of a view, suitable for hashing.

    ND7 ONE_WAY canonisation is applied ONLY to the SYSTEM view (the role
    mirror between boards of the same node is a SYSTEM concept). BASE and
    full views keep the raw direction so that real contract signals
    (uplink vs downlink on BASE channels) are preserved.
    """
    apply_one_way = (view.name == "combus_system")
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

    view_name: which view was hashed (combus, combus_system, combus_base).
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
 * Hash inputs: id, type, scope, direction, value, in wire order
 *              (independent of file order, YAML formatting, requires,
 *              theme, infoName).
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
 * combus_system_md5.h, combus_base_md5.h). Do NOT include this header
 * from application code — go through the view-specific header that
 * matches your view.
 *
 * Regenerate on every PlatformIO build via the combus_builder extra_script.
 * Do not commit this file to the source tree.
 ******************************************************************************/
"""

# Defaults used when the caller gives no version (tests, ad-hoc calls).
# The hook and the CLI read the real values from project_version.h.
_PROJECT_VERSION_MAJOR_PLACEHOLDER = 0
_PROJECT_VERSION_MINOR_PLACEHOLDER = 1


class ProjectVersionError(Exception):
    """project_version.h is missing or does not define a valid version."""


def read_project_version(project_root) -> tuple[int, int]:
    """Read (major, minor) from <project_root>/project_version.h.

    Parses `PROJECT_VERSION_MAJOR` / `PROJECT_VERSION_MINOR` constants
    (`= 0u;`). Each must be an integer in 0..255 (uint8 on the wire).
    Raises ProjectVersionError when the file or a constant is missing
    or out of range: a silent fallback would put a wrong version in the
    handshake.
    """
    path = Path(project_root) / "project_version.h"
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as e:
        raise ProjectVersionError(f"cannot read {path}: {e}") from e
    # Drop comments so a commented-out definition is not picked up.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    values = []
    for name in ("PROJECT_VERSION_MAJOR", "PROJECT_VERSION_MINOR"):
        m = re.search(rf"\b{name}\s*=\s*(\d+)\s*u?\s*;", text)
        if m is None:
            raise ProjectVersionError(f"{path}: {name} not found")
        value = int(m.group(1))
        if not 0 <= value <= 255:
            raise ProjectVersionError(f"{path}: {name}={value} is outside 0..255")
        values.append(value)
    return values[0], values[1]


# Mapping view_name -> (md5 symbol name, hex symbol name).
# The view_name is also used to capitalise the file name.
_VIEW_SYMBOLS = {
    "combus":        ("kCombusComBusMd5",        "kCombusComBusMd5Hex"),
    "combus_system":  ("kCombusSystemComBusMd5",   "kCombusSystemComBusMd5Hex"),
    "combus_base": ("kCombusBaseComBusMd5",  "kCombusBaseComBusMd5Hex"),
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
    body.append("/// Project version (read from project_version.h by the hook and the CLI).")
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
      - combus_base_md5.h   (kCombusBaseComBusMd5[16] + Hex)
      - combus_system_md5.h    (kCombusSystemComBusMd5[16]  + Hex)
      - combus_md5.h          (kCombusComBusMd5[16]       + Hex)
      - combus_wire_common.h  (kProjectVersionMajor/Minor,
                               kCombusHandshakeWirePayloadLen)

    Returns: dict[view_name -> ViewHash], list[Path]
    """
    hashes = {
        "combus_base": compute_view_hash(sel.base),
        "combus_system":  compute_view_hash(sel.system),
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
