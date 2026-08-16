#!/usr/bin/env python3
"""
combus_builder/generator.py - A6: C++ artifact generation.

Scope (A6):
  - Take the canonised ChannelDefinitions (A5.1) + BuildContext (A4).
  - Resolve `requires` against BuildContext (A8 territory but the
    gating logic is a simple subset check; A6 owns it because no
    generation can happen without it).
  - Select which channels belong to which view:
      * combus       = REMOTE + LOCAL + SYSTEM (full node view)
      * combus_remote = REMOTE only (wire view)
  - Allocate deterministic numeric IDs per view, with the `WIRE_END`
    sentinel at the boundary between REMOTE and LOCAL+SYSTEM.
  - Emit three C++ artifacts per view:
      * <view>_ids.h   - enum class only (enum + CH_COUNT)
      * <view>.h       - extern arrays + bus instance
      * <view>.cpp     - array definitions + bus instance definition
  - All output goes into a build directory (NOT in src/).

Out of scope (A6):
  - ChanLayer refactor (audit warned: layer is functionally used).
  - runLevelLayer / battLowLayer migration.
  - chains: (Phase C).
  - MD5 (A7).

Direction representation (C++):
  - `enum class Direction : uint8_t { None, Uplink, Downlink, Both=3 }`
  - `Both` = static_cast<Direction>(Uplink|Downlink) = 3.
  - Operators: `|`, `&`, `~` on the enum.
  - ChannelDescriptor stores the bitset as a single uint8_t.
  - Conversion: uplink-only -> 1, downlink-only -> 2, both -> 3, none -> 0.

View contract:
  - combus (REMOTE+LOCAL+SYSTEM):
      * ID range [0, CH_COUNT) where REMOTE is [0, WIRE_END) and
        LOCAL+SYSTEM is [WIRE_END, CH_COUNT).
      * All channels participate in the runtime bus.
  - combus_remote (REMOTE only):
      * ID range [0, CH_COUNT) where WIRE_END == CH_COUNT.
      * Used only for serial protocol sizing (frame size).
      * No sentinel beyond CH_COUNT.

ID allocation policy:
  - Within a view, IDs are allocated in stable order:
      * Sort key: (scope priority [REMOTE<LOCAL<SYSTEM], type=analog|then digital, theme, id)
  - Between views, the SAME channel always gets the SAME numeric ID.
    That way, move code from combus.h to combus_remote.h can use
    the same identifiers.
  - `WIRE_END` is the index of the first non-REMOTE channel in the
    full view (combus). In the remote view, CH_COUNT == count of REMOTE.

Generaton directory:
  - Default: .pio/build/<env>/combus_generated/
  - Configurable via:
      * CLI flag --out-dir
  - Files are written atomically (write to .tmp, then rename).
  - On any generation failure, partial files are NOT left behind
    (best-effort cleanup).
"""

from __future__ import annotations

import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .canon.channels import ChannelDefinition
from .flags import BuildContext


# =============================================================================
# ERRORS
# =============================================================================

class GeneratorError(Exception):
    """Base class for A6 generator errors."""


class RequiresResolutionError(GeneratorError):
    """A channel requires a flag that is not present in BuildContext."""

    def __init__(self, channel_id: str, missing: list[str], path: Path):
        self.channel_id = channel_id
        self.missing = missing
        self.path = path
        super().__init__(
            f"channel {channel_id!r} ({path}) requires {missing!r} "
            f"but BuildContext does not define them"
        )


# =============================================================================
# C++ DIRECTION REPRESENTATION
# =============================================================================

class Direction:
    """
    C++ representation of `direction` (frozenset -> uint8_t).

    Bit layout:
      bit 0 = uplink
      bit 1 = downlink
      0 = none, 1 = uplink, 2 = downlink, 3 = both.

    Constants are exported as Ints (uint8_t) so they can be used
    directly in the generated C++ code without depending on the
    enum class.
    """

    NONE = 0
    UPLINK = 1
    DOWNLINK = 2
    BOTH = 3

    @staticmethod
    def from_frozenset(d: frozenset[str]) -> int:
        """Convert a normalised frozenset {"uplink", "downlink"} to a uint8_t."""
        bits = 0
        if "uplink" in d:
            bits |= 1
        if "downlink" in d:
            bits |= 2
        return bits

    @staticmethod
    def to_cpp_enum_name(value: int) -> str:
        """Render a uint8_t bitmask as a C++ Direction enum value name."""
        return {
            0: "Direction::None",
            1: "Direction::Uplink",
            2: "Direction::Downlink",
            3: "Direction::Both",
        }[value]


# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass(frozen=True)
class ViewChannel:
    """
    A channel resolved for a specific view.

    numeric_id: sequential [0..CH_COUNT) within the view.
    ch:         the original A5.1 channel definition.
    direction_bits: 0/1/2/3 from Direction.from_frozenset.
    """

    numeric_id: int
    ch: ChannelDefinition
    direction_bits: int


@dataclass(frozen=True)
class View:
    """
    One resolved view (combus OR combus_remote).

    name:        "combus" or "combus_remote".
    channels:    list of ViewChannel in ID order.
    wire_end:    index of the first non-REMOTE channel (== len if no LOCAL/SYSTEM).
    ch_count:    total channel count in this view.
    """

    name: str
    channels: list[ViewChannel]
    wire_end: int

    @property
    def ch_count(self) -> int:
        return len(self.channels)


@dataclass(frozen=True)
class ViewSelection:
    """
    Both views in one struct, sharing the same ID space.

    full:   combus (REMOTE + LOCAL + SYSTEM)
    remote: combus_remote (REMOTE only)
    """

    full: View
    remote: View


# =============================================================================
# STEP 1: requires RESOLUTION
# =============================================================================

def resolve_requires(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
) -> list[ChannelDefinition]:
    """
    Filter out channels whose `requires` is not satisfied by BuildContext.

    A channel is kept iff every flag in its `requires` set is present
    in ctx (with or without value). Channel order is preserved.

    Raises RequiresResolutionError on the first channel whose
    `requires` references an undefined flag. We do NOT print a warning
    and continue: a required flag missing from BuildContext is a hard
    error (the channel must be filtered, but the build must also fail
    loudly so the operator knows the YAML is asking for something
    the build doesn't provide).
    """
    resolved: list[ChannelDefinition] = []
    for ch in channels:
        if not ch.requires:
            resolved.append(ch)
            continue
        missing = sorted(req for req in ch.requires
                         if not (ctx.has(req) or ctx.value_of(req) is not None))
        if missing:
            raise RequiresResolutionError(ch.id, missing, ch.source_path)
        resolved.append(ch)
    return resolved


# =============================================================================
# STEP 2: VIEW SELECTION + ID ALLOCATION
# =============================================================================

# Scope order for the COMBINED view (combus): REMOTE first, then LOCAL, then SYSTEM.
# This preserves the legacy wire-end ordering (REMOTE channels occupy
# indices [0..WIRE_END) so the wire codec doesn't change).
_VIEW_SCOPE_ORDER: dict[str, int] = {
    "REMOTE": 0,
    "LOCAL": 1,
    "SYSTEM": 2,
}


def _select_view(
    channels: list[ChannelDefinition],
    view_name: str,
    allowed_scopes: set[str],
) -> View:
    """
    Select channels whose scope is in allowed_scopes, allocate deterministic
    IDs in canonical order, and compute WIRE_END.

    allowed_scopes is the set of scopes that belong to this view.
    For combus: {"REMOTE", "LOCAL", "SYSTEM"}.
    For combus_remote: {"REMOTE"}.

    The ID allocation is deterministic: scope priority (REMOTE first),
    then type (analog before digital), then theme, then id.

    The first non-REMOTE channel defines WIRE_END.
    """
    selected = [ch for ch in channels if ch.scope in allowed_scopes]
    selected.sort(key=lambda ch: (
        _VIEW_SCOPE_ORDER[ch.scope],
        0 if ch.type == "analog" else 1,
        ch.theme,
        ch.id,
    ))

    view_channels: list[ViewChannel] = []
    wire_end = 0
    for idx, ch in enumerate(selected):
        view_channels.append(ViewChannel(
            numeric_id=idx,
            ch=ch,
            direction_bits=Direction.from_frozenset(ch.direction),
        ))
        if ch.scope != "REMOTE" and wire_end == 0:
            wire_end = idx
    if wire_end == 0 and selected:
        # All REMOTE: WIRE_END == CH_COUNT.
        wire_end = len(selected)

    return View(name=view_name, channels=view_channels, wire_end=wire_end)


def select_views(channels: list[ChannelDefinition]) -> ViewSelection:
    """
    Build the two views from the canonical channel list.

    Both views share the same ID space (a channel has the same numeric
    ID in both views if it appears in both).

    Returns:
      ViewSelection with .full (combus) and .remote (combus_remote).
    """
    full = _select_view(channels, "combus",
                        {"REMOTE", "LOCAL", "SYSTEM"})
    remote = _select_view(channels, "combus_remote",
                          {"REMOTE"})
    return ViewSelection(full=full, remote=remote)


# =============================================================================
# STEP 3: C++ CODE GENERATION
# =============================================================================

_HEADER_PROLOGUE = """\
/******************************************************************************
 * GENERATED FILE - DO NOT EDIT.
 *
 * Generated by scripts/combus_builder/generator.py (A6).
 * Source: <generator_version>
 * View: <view_name>
 * Build context: <ctx_summary>
 *
 * Regenerate on every PlatformIO build via the combus_builder extra_script.
 * Do not commit this file to the source tree.
 ******************************************************************************/
#pragma once
"""


def _ctx_summary(ctx: BuildContext) -> str:
    """Short human-readable string for the BuildContext."""
    parts = [f"defines={len(ctx.defines)}", f"values={len(ctx.defines_with_value)}"]
    return ", ".join(parts)


def _escape_cpp_string(s: str) -> str:
    """Escape a string for embedding in a C++ double-quoted literal."""
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "\\r")


def _render_ids_header(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>_ids.h` file.

    Contains:
      - enum class <View>ComBusID : uint8_t { ... CH_COUNT };
    """
    # Group by type for readability (analog/digital).
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]

    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("// =============================================================================")
    body.append("// 1. ENUMS")
    body.append("// =============================================================================")
    body.append("")

    if analog:
        body.append(f"enum class Analog{view.name.capitalize()}ID : uint8_t {{")
        body.append("    // --- analog channels ---")
        for vc in analog:
            body.append(f"    {vc.ch.id} = {vc.numeric_id},")
        body.append("    CH_COUNT")
        body.append("};")
        body.append("")

    if digital:
        body.append(f"enum class Digital{view.name.capitalize()}ID : uint8_t {{")
        body.append("    // --- digital channels ---")
        for vc in digital:
            body.append(f"    {vc.ch.id} = {vc.numeric_id},")
        body.append("    CH_COUNT")
        body.append("};")
        body.append("")

    # WIRE_END: only meaningful for the full view.
    if view.name == "combus":
        body.append(f"// Index of the first non-REMOTE channel (REMOTE = [0..WIRE_END)).")
        body.append(f"static constexpr uint8_t {view.name.capitalize()}WireEnd = {view.wire_end}u;")
        body.append("")

    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_header(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>.h` file.

    Contains:
      - Direction enum (None, Uplink, Downlink, Both)
      - ChannelDescriptor struct (infoName, value, direction)
      - extern arrays + bus instance
    """
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("#include <cstdint>")
    body.append("")
    body.append(f"#include \"{view.name}_ids.h\"")
    body.append("")
    body.append("// =============================================================================")
    body.append("// 1. DIRECTION (C++ representation of A5.1 `direction`)")
    body.append("// =============================================================================")
    body.append("")
    body.append("/**")
    body.append(" * @brief Wire direction of a channel (bitmask, 1 byte).")
    body.append(" *")
    body.append(" * @details bit 0 = uplink, bit 1 = downlink.")
    body.append(" *   None     = 0")
    body.append(" *   Uplink   = 1")
    body.append(" *   Downlink = 2")
    body.append(" *   Both     = 3 (= Uplink | Downlink)")
    body.append(" */")
    body.append("enum class Direction : uint8_t {")
    body.append("    None     = 0,")
    body.append("    Uplink   = 1,")
    body.append("    Downlink = 2,")
    body.append("    Both     = 3")
    body.append("};")
    body.append("")
    body.append("static constexpr Direction operator|(Direction a, Direction b) {")
    body.append("    return static_cast<Direction>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));")
    body.append("}")
    body.append("")
    body.append("static constexpr Direction operator&(Direction a, Direction b) {")
    body.append("    return static_cast<Direction>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));")
    body.append("}")
    body.append("")
    body.append("static constexpr Direction operator~(Direction a) {")
    body.append("    return static_cast<Direction>(~static_cast<uint8_t>(a) & 0x03u);")
    body.append("}")
    body.append("")
    body.append("// =============================================================================")
    body.append("// 2. CHANNEL DESCRIPTORS")
    body.append("// =============================================================================")
    body.append("")
    body.append("struct ChannelDescriptor {")
    body.append("    const char* infoName;")
    body.append("    Direction   direction;")
    body.append("};")
    body.append("")
    body.append("// =============================================================================")
    body.append("// 3. EXTERN ARRAYS")
    body.append("// =============================================================================")
    body.append("")
    if any(vc.ch.type == "analog" for vc in view.channels):
        body.append(f"extern const ChannelDescriptor Analog{view.name.capitalize()}ChannelDescriptors[];")
        body.append(f"extern const uint8_t Analog{view.name.capitalize()}ChannelCount;")
    if any(vc.ch.type == "digital" for vc in view.channels):
        body.append(f"extern const ChannelDescriptor Digital{view.name.capitalize()}ChannelDescriptors[];")
        body.append(f"extern const uint8_t Digital{view.name.capitalize()}ChannelCount;")
    body.append("")
    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_source(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>.cpp` file.

    Contains:
      - ChannelDescriptor arrays (one per type).
      - Channel counts.
    """
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append(f"#include \"{view.name}.h\"")
    body.append("")
    body.append("// =============================================================================")
    body.append("// 1. ANALOG CHANNEL DESCRIPTORS")
    body.append("// =============================================================================")
    body.append("")
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]
    cap = view.name.capitalize()
    if analog:
        body.append(f"const ChannelDescriptor Analog{cap}ChannelDescriptors[] = {{")
        for vc in analog:
            dir_name = Direction.to_cpp_enum_name(vc.direction_bits)
            escaped_info = _escape_cpp_string(vc.ch.info_name)
            body.append(f"    {{ \"{escaped_info}\", {dir_name} }}, // {vc.ch.id}")
        body.append("};")
        body.append(f"const uint8_t Analog{cap}ChannelCount = {len(analog)}u;")
        body.append("")
    body.append("// =============================================================================")
    body.append("// 2. DIGITAL CHANNEL DESCRIPTORS")
    body.append("// =============================================================================")
    body.append("")
    if digital:
        body.append(f"const ChannelDescriptor Digital{cap}ChannelDescriptors[] = {{")
        for vc in digital:
            dir_name = Direction.to_cpp_enum_name(vc.direction_bits)
            escaped_info = _escape_cpp_string(vc.ch.info_name)
            body.append(f"    {{ \"{escaped_info}\", {dir_name} }}, // {vc.ch.id}")
        body.append("};")
        body.append(f"const uint8_t Digital{cap}ChannelCount = {len(digital)}u;")
        body.append("")
    body.append("// EOF")
    return "\n".join(body) + "\n"


# =============================================================================
# STEP 4: FILE EMISSION
# =============================================================================

def _safe_write(path: Path, content: str) -> None:
    """Write content to path atomically (write to .tmp, then rename)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_fd, tmp_path = tempfile.mkstemp(
        prefix=path.name + ".", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(tmp_fd, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        os.replace(tmp_path, path)
    except Exception:
        # Best-effort cleanup of the temp file.
        try:
            os.unlink(tmp_path)
        except OSError:
            pass
        raise


def emit_view(view: View, out_dir: Path, ctx: BuildContext) -> list[Path]:
    """
    Emit the three files for a view into out_dir.

    Returns the list of written paths.
    """
    written: list[Path] = []
    files = {
        f"{view.name}_ids.h": _render_ids_header(view, ctx),
        f"{view.name}.h": _render_header(view, ctx),
        f"{view.name}.cpp": _render_source(view, ctx),
    }
    for name, content in files.items():
        p = out_dir / name
        _safe_write(p, content)
        written.append(p)
    return written


def generate(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
    out_dir: Path,
) -> ViewSelection:
    """
    Full A6 generation pipeline.

    1. Resolve `requires` against ctx.
    2. Select views.
    3. Emit three files per view.

    Returns the ViewSelection so callers (tests, diagnostics) can
    inspect what was generated.
    """
    active = resolve_requires(channels, ctx)
    sel = select_views(active)
    emit_view(sel.full, out_dir, ctx)
    emit_view(sel.remote, out_dir, ctx)
    return sel


# =============================================================================
# CLI / diagnostic
# =============================================================================

def main(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
    out_dir: Path,
) -> int:
    """CLI entry point: generate artifacts and print a summary."""
    try:
        sel = generate(channels, ctx, out_dir)
    except GeneratorError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    print(f"[combus_builder] generated view 'combus' "
          f"({sel.full.ch_count} channels, WIRE_END={sel.full.wire_end}) "
          f"-> {out_dir}/combus.{{h,cpp,_ids.h}}")
    print(f"[combus_builder] generated view 'combus_remote' "
          f"({sel.remote.ch_count} channels, WIRE_END={sel.remote.wire_end}) "
          f"-> {out_dir}/combus_remote.{{h,cpp,_ids.h}}")
    return 0
