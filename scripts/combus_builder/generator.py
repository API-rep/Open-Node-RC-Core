#!/usr/bin/env python3
"""
combus_builder/generator.py - A6.1: C++ artifact generation.

A6.1 corrections vs A6:
  - REMOVED ChannelDescriptor abstraction.
  - Generator now emits REAL runtime structures (AnalogComBus / DigitalComBus)
    that match the runtime in include/struct/combus_struct.h.
  - `direction` is added as a fourth field on those structs (alongside
    `infoName`, `value`, `layer`). `ChanLayer` is preserved unchanged
    (audit A6 confirmed `_layer_ok()` requires it).
  - Both views still produced:
      * combus        = REMOTE + LOCAL + SYSTEM
      * combus_remote = REMOTE only

Scope (A6.1):
  - Take canonised ChannelDefinitions (A5.1) + BuildContext (A4).
  - Resolve `requires` against BuildContext.
  - Allocate deterministic numeric IDs per view.
  - Emit the real runtime structures, not a new abstraction.

Out of scope (A6.1):
  - ChanLayer refactor (audit warned: layer is functionally used).
  - runLevelLayer / battLowLayer migration.
  - chains: (Phase C).
  - MD5 (A7).
  - .cbch.
  - Full .inc migration (only the new .cb files are used by the generator;
    the .inc files remain in the source tree until Phase D).

Direction representation (C++):
  - `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2, Both=3 }`
  - Bit layout: bit 0 = uplink, bit 1 = downlink.
  - Both = static_cast<Direction>(Uplink|Downlink) = 3.
  - Operators `|`, `&`, `~` are defined in combus_struct.h.

View contract:
  - combus (REMOTE+LOCAL+SYSTEM):
      * ID range [0, CH_COUNT) where REMOTE is [0, WIRE_END) and
        LOCAL+SYSTEM is [WIRE_END, CH_COUNT).
      * Generated artifacts:
          - combus_ids.h        (enum class only)
          - combus.h            (extern arrays + bus instance)
          - combus.cpp          (array definitions + bus instance definition)
  - combus_remote (REMOTE only):
      * ID range [0, CH_COUNT) where WIRE_END == CH_COUNT.
      * Generated artifacts:
          - combus_remote_ids.h
          - combus_remote.h
          - combus_remote.cpp
      * No sentinel beyond CH_COUNT.

ID allocation policy:
  - Within a view, IDs are allocated in stable order:
      * Sort key: (scope priority [REMOTE<LOCAL<SYSTEM], type=analog|then digital, theme, id)
  - Between views, the SAME channel always gets the SAME numeric ID.

Generation directory:
  - Default: .pio/build/<env>/combus_generated/
  - Configurable via CLI flag --out-dir
  - Files are written atomically (write to .tmp, then rename).
"""

from __future__ import annotations

import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from .canon.channels import ChannelDefinition
from .flags import BuildContext


# =============================================================================
# ERRORS
# =============================================================================

class GeneratorError(Exception):
    """Base class for A6.1 generator errors."""


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
# C++ DIRECTION REPRESENTATION (matches combus_struct.h Direction enum)
# =============================================================================

class Direction:
    """
    C++ representation of `direction` (frozenset -> uint8_t).

    Bit layout: bit 0 = uplink, bit 1 = downlink.
    Values match the C++ `enum class Direction : uint8_t` in combus_struct.h:
      None=0, Uplink=1, Downlink=2, Both=3.

    This class is the single source of truth for the Python-side mapping.
    A coherence check (test_python_direction_matches_cpp_enum) verifies that
    the constants match the values emitted in the generated C++ header.
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
    All three views in one struct, sharing the same ID space.

    full:   combus        (REMOTE + LOCAL + SYSTEM)
    local:  combus_local  (REMOTE + LOCAL)        — A6.2
    remote: combus_remote (REMOTE only)

    A channel that appears in multiple views has the SAME numeric_id in
    every view it appears in (no renumbering between views).
    """

    full: View
    local: View
    remote: View


# =============================================================================
# SCOPE -> ChanLayer C++ MAPPING
# =============================================================================

# ComBus scope -> ChanLayer C++ enum.
# Maps the YAML scope to the runtime ChanLayer value used in the
# generated `AnalogComBus` / `DigitalComBus` initializers.
_SCOPE_TO_CHANLAYER: dict[str, str] = {
    "REMOTE": "ChanLayer::REMOTE",
    "LOCAL":  "ChanLayer::LOCAL",
    "SYSTEM": "ChanLayer::SYSTEM",
}


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
    `requires` references an undefined flag.
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
    Build the three views from the canonical channel list.

    All views share the same ID space (a channel has the same numeric
    ID in every view it appears in — no renumbering between views).

    The combus_local view (A6.2) is a strict prefix of combus: it
    contains exactly the REMOTE+LOCAL channels of combus, in the same
    order, with the same numeric IDs. This is by construction because
    the sort key is identical and the prefix is contiguous.
    """
    full = _select_view(channels, "combus",
                        {"REMOTE", "LOCAL", "SYSTEM"})
    local = _select_view(channels, "combus_local",
                         {"REMOTE", "LOCAL"})
    remote = _select_view(channels, "combus_remote",
                          {"REMOTE"})
    return ViewSelection(full=full, local=local, remote=remote)


# =============================================================================
# STEP 3: C++ CODE GENERATION
# =============================================================================

_HEADER_PROLOGUE = """\
/******************************************************************************
 * GENERATED FILE - DO NOT EDIT.
 *
 * Generated by scripts/combus_builder/generator.py (A6.1).
 * Source: <generator_version>
 * View: <view_name>
 * Build context: <ctx_summary>
 *
 * Regenerate on every PlatformIO build via the combus_builder extra_script.
 * Do not commit this file to the source tree.
 ******************************************************************************/
"""


def _ctx_summary(ctx: BuildContext) -> str:
    """Short human-readable string for the BuildContext."""
    parts = [f"defines={len(ctx.defines)}", f"values={len(ctx.defines_with_value)}"]
    return ", ".join(parts)


def _escape_cpp_string(s: str) -> str:
    """Escape a string for embedding in a C++ double-quoted literal."""
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "\\r")


def _default_value_for_type(type_: str) -> str:
    """
    Return the C++ literal for the default `value` of a freshly-generated
    channel WHEN THE YAML CHANNEL DEFINES NO `value:` (matches the
    runtime convention used in the legacy .inc files).

    Analog defaults to CbusNeutral (neutral 16-bit value). Digital defaults
    to false. The runtime may override these at boot via combus_set_*().

    Note : when the YAML declares an explicit `value`, the canonical
    representation in ChannelDefinition.value is used instead — see
    `_render_value`.
    """
    if type_ == "analog":
        return "CbusNeutral"
    return "false"


# Mapping canon → littéral C++ pour les tokens `value:` acceptés.
# Les tokens sont émis tels quels (ce sont déjà des symboles C++ valides
# ou des littéraux booléens).
_VALUE_TOKEN_TO_CPP: dict[str, str] = {
    # analog
    "CbusMinVal":  "CbusMinVal",
    "CbusNeutral": "CbusNeutral",
    "CbusMaxVal":  "CbusMaxVal",
    # digital
    "low":  "false",
    "high": "true",
}


def _render_value(ch: ChannelDefinition, type_: str) -> str:
    """
    Return the C++ literal for the `value` field of a freshly-generated
    channel.

    Resolution order :
      1. If `ch.value is None` (YAML absent) → use the generator default
         (analog: CbusNeutral, digital: false). Behaviour preserved from
         pre-A5.x to keep legacy parity.
      2. If `ch.value` is a string token (e.g. 'CbusNeutral', 'high') →
         emit the mapped C++ literal (see _VALUE_TOKEN_TO_CPP).
      3. If `ch.value` is an int (analog only) → emit `<n>u` (uint16_t).

    Any other shape is a programming error : _validate_value in
    canon/channels.py rejects anything else at the canonisation layer.
    """
    if ch.value is None:
        return _default_value_for_type(type_)

    # Token (str)
    if isinstance(ch.value, str):
        try:
            return _VALUE_TOKEN_TO_CPP[ch.value]
        except KeyError as e:
            # Should be unreachable : _validate_value rejects unknown tokens.
            raise GeneratorError(
                f"unknown value token {ch.value!r} for channel "
                f"{ch.id!r} (type={type_}); this is a bug — "
                f"canon/channels.py should have rejected it."
            ) from e

    # Integer (analog only, validated by _validate_value)
    if isinstance(ch.value, int) and not isinstance(ch.value, bool):
        if type_ != "analog":
            # Should be unreachable : _validate_value rejects int on digital.
            raise GeneratorError(
                f"integer value {ch.value!r} for digital channel "
                f"{ch.id!r}; this is a bug — canon/channels.py should "
                f"have rejected it."
            )
        return f"{ch.value}u"

    # Should be unreachable.
    raise GeneratorError(
        f"unsupported canonical value {ch.value!r} (type={type(ch.value).__name__}) "
        f"for channel {ch.id!r}; this is a bug in the canon<->generator contract."
    )


def _render_ids_header(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>_ids.h` file.

    Contains:
      - enum class Analog<View>ComBusID : uint8_t { ... CH_COUNT };
      - enum class Digital<View>ComBusID : uint8_t { ... CH_COUNT };
    """
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]

    cap = view.name.capitalize()
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6.1")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("#include <cstdint>")
    body.append("")
    body.append("// =============================================================================")
    body.append("// 1. ENUMS")
    body.append("// =============================================================================")
    body.append("")

    if analog:
        body.append(f"enum class Analog{cap}ID : uint8_t {{")
        body.append("    // --- analog channels ---")
        for vc in analog:
            body.append(f"    {vc.ch.id} = {vc.numeric_id},")
        body.append("    CH_COUNT")
        body.append("};")
        body.append("")

    if digital:
        body.append(f"enum class Digital{cap}ID : uint8_t {{")
        body.append("    // --- digital channels ---")
        for vc in digital:
            body.append(f"    {vc.ch.id} = {vc.numeric_id},")
        body.append("    CH_COUNT")
        body.append("};")
        body.append("")

    # WIRE_END: only meaningful for the full view.
    if view.name == "combus":
        body.append(f"// Index of the first non-REMOTE channel (REMOTE = [0..WIRE_END)).")
        body.append(f"static constexpr uint8_t {cap}WireEnd = {view.wire_end}u;")
        body.append("")

    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_header(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>.h` file.

    Contains:
      - Forward declaration / inclusion of the IDs header
      - extern declarations of the channel arrays
      - extern declaration of the bus instance (combus view only)
    """
    cap = view.name.capitalize()
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6.1")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("#pragma once")
    body.append("")
    body.append(f"#include \"{view.name}_ids.h\"")
    body.append("")
    body.append("#include <struct/combus_struct.h>")
    body.append("")
    body.append("// =============================================================================")
    body.append("// 1. EXTERN ARRAYS")
    body.append("// =============================================================================")
    body.append("")
    if any(vc.ch.type == "analog" for vc in view.channels):
        body.append(
            f"extern AnalogComBus Analog{cap}Array"
            f"[static_cast<uint8_t>(Analog{cap}ID::CH_COUNT)];"
        )
    if any(vc.ch.type == "digital" for vc in view.channels):
        body.append(
            f"extern DigitalComBus Digital{cap}Array"
            f"[static_cast<uint8_t>(Digital{cap}ID::CH_COUNT)];"
        )

    # Bus instance is only declared on the combus (full) view.
    if view.name == "combus":
        body.append("")
        body.append("// =============================================================================")
        body.append("// 2. EXTERN BUS INSTANCE")
        body.append("// =============================================================================")
        body.append("")
        body.append("extern ComBus comBus;")

    body.append("")
    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_source(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>.cpp` file.

    Contains:
      - AnalogComBusArray[] (if analog channels exist)
      - DigitalComBusArray[] (if digital channels exist)
      - comBus definition (combus view only)
    """
    cap = view.name.capitalize()
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]

    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6.1")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append(f"#include \"{view.name}.h\"")
    if any(vc.ch.type == "analog" for vc in view.channels):
        body.append("#include <core/system/combus/combus_res.h>  // CbusNeutral")
    body.append("")

    if analog:
        body.append("// =============================================================================")
        body.append("// 1. ANALOG CHANNEL ARRAY")
        body.append("// =============================================================================")
        body.append("")
        body.append(
            f"AnalogComBus Analog{cap}Array"
            f"[static_cast<uint8_t>(Analog{cap}ID::CH_COUNT)] = {{"
        )
        for vc in analog:
            _emit_channel_init(body, vc, "analog")
        body.append("};")
        body.append("")

    if digital:
        body.append("// =============================================================================")
        body.append("// 2. DIGITAL CHANNEL ARRAY")
        body.append("// =============================================================================")
        body.append("")
        body.append(
            f"DigitalComBus Digital{cap}Array"
            f"[static_cast<uint8_t>(Digital{cap}ID::CH_COUNT)] = {{"
        )
        for vc in digital:
            _emit_channel_init(body, vc, "digital")
        body.append("};")
        body.append("")

    if view.name == "combus":
        body.append("// =============================================================================")
        body.append("// 3. BUS INSTANCE")
        body.append("// =============================================================================")
        body.append("")
        body.append("ComBus comBus {")
        body.append("    .runLevel        = RunLevel::NOT_YET_SET,")
        body.append("    .runLevelLayer   = ChanLayer::LOCAL,")
        body.append(f"    .analogBus       = Analog{cap}Array,")
        body.append(f"    .digitalBus      = Digital{cap}Array,")
        body.append("    .analogBusMaxVal = (1UL << (sizeof(uint16_t) * 8)) - 1")
        body.append("};")
        body.append("")

    body.append("// EOF")
    return "\n".join(body) + "\n"


def _emit_channel_init(body: list[str], vc: ViewChannel, type_: str) -> None:
    """
    Emit a single `AnalogComBus` / `DigitalComBus` initializer entry.

    Output format (matches runtime AnalogComBus/DigitalComBus struct):

        { .infoName = "...", .value = ..., .layer = ChanLayer::X, .direction = Direction::Y },

    The `.value` field is rendered from the canonical channel value
    (see _render_value). It honours the YAML `value:` contract :
      - analog : CbusMinVal / CbusNeutral / CbusMaxVal / `<n>u`
      - digital : false / true
    Falls back to the generator default when `value:` is absent.

    The `.layer` field comes from the channel's scope (REMOTE/LOCAL/SYSTEM).
    The `.direction` field is the bitmask from Direction.from_frozenset.
    """
    escaped_info = _escape_cpp_string(vc.ch.info_name)
    layer = _SCOPE_TO_CHANLAYER[vc.ch.scope]
    dir_name = Direction.to_cpp_enum_name(vc.direction_bits)
    value_literal = _render_value(vc.ch, type_)
    body.append(
        f"    {{ .infoName = \"{escaped_info}\", "
        f".value = {value_literal}, "
        f".layer = {layer}, "
        f".direction = {dir_name} }}, // {vc.ch.id}"
    )


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
    Full A7 generation pipeline.

    1. Resolve `requires` against ctx.
    2. Select views (combus, combus_local, combus_remote).
    3. Emit three files per view (9 files total).
    4. Emit MD5 artifacts for combus_local and combus_remote (A7).
       combus (full) is intentionally NOT hashed (see A7 spec).

    Returns the ViewSelection so callers (tests, diagnostics) can
    inspect what was generated.
    """
    active = resolve_requires(channels, ctx)
    sel = select_views(active)
    emit_view(sel.full, out_dir, ctx)
    emit_view(sel.local, out_dir, ctx)
    emit_view(sel.remote, out_dir, ctx)
    # A7.1: MD5 artifact for combus_remote only.
    # projectVersion is a placeholder until project_version.h is introduced
    # (out of A7 scope — see md5.py docstring).
    from .md5 import generate_md5_artifacts
    generate_md5_artifacts(sel, out_dir)
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
    print(f"[combus_builder] generated view 'combus_local' "
          f"({sel.local.ch_count} channels, WIRE_END={sel.local.wire_end}) "
          f"-> {out_dir}/combus_local.{{h,cpp,_ids.h}}")
    print(f"[combus_builder] generated view 'combus_remote' "
          f"({sel.remote.ch_count} channels, WIRE_END={sel.remote.wire_end}) "
          f"-> {out_dir}/combus_remote.{{h,cpp,_ids.h}}")
    return 0
