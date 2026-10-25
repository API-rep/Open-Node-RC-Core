#!/usr/bin/env python3
"""
combus_builder/generator.py - A6.1: C++ artifact generation.

A6.1 corrections vs A6:
  - REMOVED ChannelDescriptor abstraction.
  - Generator now emits REAL runtime structures (AnalogComBus / DigitalComBus)
    that match the runtime in src/core/system/combus/combus_defs.h.
  - `direction` is added as a fourth field on those structs (alongside
    `infoName`, `value`, `layer`). `ChanLayer` is preserved unchanged
    (audit A6 confirmed `_layer_ok()` requires it).
  - Both views still produced:
      * combus        = BASE + SYSTEM + BOARD
      * combus_base = BASE only

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
  - Full .inc migration: done (the .inc files no longer exist).

Direction representation (C++):
  - `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2, Both=3 }`
  - Bit layout: bit 0 = uplink, bit 1 = downlink.
  - Both = static_cast<Direction>(Uplink|Downlink) = 3.
  - No bitwise operators are defined on Direction in the C++ code.

View contract:
  - combus (BASE+SYSTEM+BOARD):
      * ID range [0, CH_COUNT) where BASE is [0, WIRE_END) and
        SYSTEM+BOARD is [WIRE_END, CH_COUNT).
      * Generated artifacts:
          - combus_ids.h        (enum class only)
          - combus.h            (extern arrays + bus instance)
          - combus.cpp          (array definitions + bus instance definition)
  - combus_base (BASE only):
      * ID range [0, CH_COUNT) where WIRE_END == CH_COUNT.
      * Generated artifacts:
          - combus_base_ids.h
          - combus_base.h
          - combus_base.cpp
      * No sentinel beyond CH_COUNT.

ID allocation policy:
  - Within a view, IDs are allocated in stable order:
      * analog and digital are numbered independently (one counter per type);
        within a type the sort key is (scope priority [BASE<SYSTEM<BOARD], theme, id)
  - Between views, the SAME channel always gets the SAME numeric ID.

Generation directory:
  - Hook default: <PROJECT_BUILD_DIR>/combus_generated/ ; CLI default:
    <project-root>/out/combus_generated/
  - Configurable via CLI flag --out-dir
  - Files are written atomically (write to .tmp, then rename).

ND1 (multiboard):
  - generate() accepts an optional `filename_prefix: str = ""` parameter.
  - When non-empty, every emitted file is renamed with the prefix
    inserted between the view name and the suffix:
        combus_ids.h        -> combus_<prefix>_ids.h
        combus.h            -> combus_<prefix>.h
        combus.cpp          -> combus_<prefix>.cpp
        combus_system_ids.h  -> combus_system_<prefix>_ids.h
        combus_base_ids.h -> combus_base_<prefix>_ids.h
        combus_md5.h        -> combus_<prefix>_md5.h
        combus_system_md5.h  -> combus_system_<prefix>_md5.h
        combus_base_md5.h -> combus_base_<prefix>_md5.h
    combus_wire_common.h is NOT prefixed (it is a shared header).
  - The C++ enum identifiers (AnalogComBusID, etc.) are NOT prefixed —
    they remain stable across boards so application code does not need
    to change per board.
  - The `#include` directives inside the generated .h/.cpp files are
    updated to reference the prefixed filenames.
  - No selection / sort / numbering logic is touched.
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
    """A channel requires a flag that is not present in BuildContext.

    No longer raised by resolve_requires() (since 2026-10-22, a channel whose
    flags are missing is skipped and reported). Kept for compatibility.
    """

    def __init__(self, channel_id: str, missing: list[str], path: Path):
        self.channel_id = channel_id
        self.missing = missing
        self.path = path
        super().__init__(
            f"channel {channel_id!r} ({path}) requires {missing!r} "
            f"but BuildContext does not define them"
        )


# =============================================================================
# C++ DIRECTION REPRESENTATION (matches combus_defs.h Direction enum)
# =============================================================================

class Direction:
    """
    C++ representation of `direction` (frozenset -> uint8_t).

    Bit layout: bit 0 = uplink, bit 1 = downlink.
    Values match the C++ `enum class Direction : uint8_t` in combus_defs.h:
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
    One resolved view (combus OR combus_base).

    name:             "combus" or "combus_base".
    channels:         list of ViewChannel in ID order.
    wire_end:         index of the first non-BASE channel (== len if no SYSTEM/BOARD).
    wire_end_analog:  count of BASE analog channels (== number of BASE analog
                      channels in canonical order).  Used by the C++ generator
                      to emit a per-bus wire-end constant (Phase 1 A.11 fix:
                      analog and digital wire-ends are NOT always equal).
    wire_end_digital: count of BASE digital channels.  See wire_end_analog.
    ch_count:         total channel count in this view.
    """

    name: str
    channels: list[ViewChannel]
    wire_end: int
    wire_end_analog: int
    wire_end_digital: int

    @property
    def ch_count(self) -> int:
        return len(self.channels)


@dataclass(frozen=True)
class ViewSelection:
    """
    All three views in one struct, sharing the same ID space.

    full:   combus        (BASE + SYSTEM + BOARD)
    system: combus_system (BASE + SYSTEM)         — A6.2
    base:   combus_base   (BASE only)

    A channel that appears in multiple views has the SAME numeric_id in
    every view it appears in (no renumbering between views).
    """

    full: View
    system: View
    base: View


# =============================================================================
# SCOPE -> ChanLayer C++ MAPPING
# =============================================================================

# ComBus scope -> ChanLayer C++ enum.
# Maps the YAML scope to the runtime ChanLayer value used in the
# generated `AnalogComBus` / `DigitalComBus` initializers.
_SCOPE_TO_CHANLAYER: dict[str, str] = {
    "BASE": "ChanLayer::BASE",
    "SYSTEM":  "ChanLayer::SYSTEM",
    "BOARD": "ChanLayer::BOARD",
}


# =============================================================================
# ND1: filename prefix helper
# =============================================================================

def _file_base(view_name: str, filename_prefix: str) -> str:
    """
    Return the file basename for a view, with the ND1 prefix applied.

    Legacy (filename_prefix == ""): returns view_name unchanged.
    Node mode (filename_prefix != ""): returns "<view_name>_<prefix>".

    Example:
        _file_base("combus", "")            -> "combus"
        _file_base("combus", "mainboard")   -> "combus_mainboard"
        _file_base("combus_system", "mainboard") -> "combus_system_mainboard"
    """
    if filename_prefix:
        return f"{view_name}_{filename_prefix}"
    return view_name


# =============================================================================
# STEP 1: requires RESOLUTION
# =============================================================================

def resolve_requires(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
    skipped: list[tuple[str, list[str]]] | None = None,
) -> list[ChannelDefinition]:
    """
    Filter out channels whose `requires` is not satisfied by BuildContext.

    A channel is kept iff every flag in its `requires` set is present
    in ctx (with or without value). Channel order is preserved. This is
    the same rule as the node mode (diffusion._is_active_for_board).

    A channel whose flags are missing is not generated. It never
    disappears silently: when `skipped` is given, each removed channel is
    appended to it as (channel id, sorted missing flags).
    """
    resolved: list[ChannelDefinition] = []
    for ch in channels:
        if not ch.requires:
            resolved.append(ch)
            continue
        missing = sorted(req for req in ch.requires
                         if not (ctx.has(req) or ctx.value_of(req) is not None))
        if missing:
            if skipped is not None:
                skipped.append((ch.id, missing))
            continue
        resolved.append(ch)
    return resolved


# =============================================================================
# STEP 2: VIEW SELECTION + ID ALLOCATION
# =============================================================================

# Scope order for the COMBINED view (combus): BASE first, then SYSTEM, then BOARD.
# This preserves the legacy wire-end ordering (BASE channels occupy
# indices [0..WIRE_END) so the wire codec doesn't change).
_VIEW_SCOPE_ORDER: dict[str, int] = {
    "BASE": 0,
    "SYSTEM": 1,
    "BOARD": 2,
}


def _select_view(
    channels: list[ChannelDefinition],
    view_name: str,
    allowed_scopes: set[str],
) -> View:
    """
    Select channels whose scope is in allowed_scopes, allocate deterministic
    IDs in canonical order, and compute WIRE_END.

    ID allocation policy (Phase 5 A15 fix — bug d'indexation):
        Each TYPE (analog, digital) has its OWN 0-based counter. The
        numeric_id assigned to a channel is its position within its type
        in the view, NOT its position in the global view-wide sequence.
        This guarantees that `DigitalComBusArray[DigitalComBusID::XXX]`
        accesses the correct slot — previously, IDs were allocated via a
        single global enumerate() which mixed analog and digital into one
        sequence, causing digital IDs to start at 9 (after the 9 analog
        BASE channels) and a complete misalignment between the enum
        values and the runtime array indices.

    allowed_scopes is the set of scopes that belong to this view.
    For combus: {"BASE", "SYSTEM", "BOARD"}.
    For combus_base: {"BASE"}.

    Sort key (stable, per-type):
        (scope priority, candidate, theme, id)
    Scope priority: BASE < SYSTEM < BOARD (preserves wire order:
    BASE channels are at the lowest indices, SYSTEM next, BOARD last).
    Link rework 2.2a: the BASE candidates come after the stable BASE
    channels of their type, so adding one never moves a stable BASE id
    (the next set is the stable set followed by the candidates).
    """
    selected = [ch for ch in channels if ch.scope in allowed_scopes]
    # Per-type sort: analog and digital are sorted independently.
    # They are concatenated later, analog-first.
    selected_analog = sorted(
        [ch for ch in selected if ch.type == "analog"],
        key=lambda ch: (
            _VIEW_SCOPE_ORDER[ch.scope],
            ch.candidate,
            ch.theme,
            ch.id,
        ),
    )
    selected_digital = sorted(
        [ch for ch in selected if ch.type == "digital"],
        key=lambda ch: (
            _VIEW_SCOPE_ORDER[ch.scope],
            ch.candidate,
            ch.theme,
            ch.id,
        ),
    )

    view_channels: list[ViewChannel] = []
    wire_end = 0
    # Phase 1 A.11 fix: per-bus wire-end counters (analog vs digital).
    # Both counters track BASE channels only, separated by type.  The
    # final values (after the loop) represent the count of BASE
    # channels of each type in the view, which may differ from
    # `wire_end` (= total BASE count across both types).
    wire_end_analog = 0
    wire_end_digital = 0
    # Phase 5 A15: separate 0-based counters per type.
    analog_idx = 0
    digital_idx = 0
    # The wire order is: all analog channels first (in scope/theme/id
    # order), then all digital channels (in scope/theme/id order). This
    # preserves the legacy "BASE channels at the lowest indices"
    # invariant: among the analog channels, BASE ones come first
    # because they sort before SYSTEM/BOARD.
    for ch in selected_analog:
        view_channels.append(ViewChannel(
            numeric_id=analog_idx,
            ch=ch,
            direction_bits=Direction.from_frozenset(ch.effective_direction),
        ))
        if ch.scope == "BASE":
            wire_end_analog += 1
        analog_idx += 1
    for ch in selected_digital:
        view_channels.append(ViewChannel(
            numeric_id=digital_idx,
            ch=ch,
            direction_bits=Direction.from_frozenset(ch.effective_direction),
        ))
        if ch.scope == "BASE":
            wire_end_digital += 1
        digital_idx += 1
    # Compute wire_end: index of the first non-BASE channel in the
    # flattened wire order. BASE channels of both types come first
    # because they sort first within each type.
    wire_end = wire_end_analog + wire_end_digital

    return View(
        name=view_name,
        channels=view_channels,
        wire_end=wire_end,
        wire_end_analog=wire_end_analog,
        wire_end_digital=wire_end_digital,
    )


def select_views(channels: list[ChannelDefinition]) -> ViewSelection:
    """
    Build the three views from the canonical channel list.

    All views share the same ID space (a channel has the same numeric
    ID in every view it appears in — no renumbering between views).

    The combus_system view (A6.2) is a strict prefix of combus: it
    contains exactly the BASE+SYSTEM channels of combus, in the same
    order, with the same numeric IDs. This is by construction because
    the sort key is identical and the prefix is contiguous.
    """
    full = _select_view(channels, "combus",
                        {"BASE", "SYSTEM", "BOARD"})
    system = _select_view(channels, "combus_system",
                         {"BASE", "SYSTEM"})
    base = _select_view(channels, "combus_base",
                          {"BASE"})
    return ViewSelection(full=full, system=system, base=base)


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
    runtime convention of the former hand-written .inc files).

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


# Mapping view.name -> C++ identifier prefix (must match legacy naming).
# Legacy uses "ComBus" (B majuscule) — str.capitalize() would yield
# "Combus" which is wrong. We hardcode the correct form here.
_VIEW_ID_PREFIX: dict[str, str] = {
    "combus": "ComBus",
    "combus_system": "ComBusSystem",
    "combus_base": "ComBusBase",
}


def _render_ids_header(view: View, ctx: BuildContext) -> str:
    """
    Render the `<view>_ids.h` file.

    Contains:
      - enum class Analog<View>ComBusID : uint8_t { ... CH_COUNT };
      - enum class Digital<View>ComBusID : uint8_t { ... CH_COUNT };
    """
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]

    cap = _VIEW_ID_PREFIX[view.name]
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6.1")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("#pragma once")  # Phase 4 (A14): guard against multiple inclusion (was missing — caused multiple-definition errors when included directly from .cpp files).
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

    # Phase 4 (A14): ComBusWireEnd / ComBusWireEndAnalog / ComBusWireEndDigital
    # are no longer emitted. Wire dimensions are now derived directly from
    # the Base view's ID enums (AnalogComBusBaseID::CH_COUNT and
    # DigitalComBusBaseID::CH_COUNT) at the call site. See:
    #   - src/machines/init/com/combus_uart_init.cpp
    #   - src/machines/main.cpp
    #   - src/machines/system/debug/dashboard_machine.cpp

    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_header(view: View, ctx: BuildContext, *, filename_prefix: str = "") -> str:
    """
    Render the `<view>.h` file.

    Contains:
      - Forward declaration / inclusion of the IDs header
      - extern declarations of the channel arrays
      - extern declaration of the bus instance (combus view only)

    `filename_prefix` (ND1): when non-empty, the `#include` directive
    targets the prefixed ids header (e.g. `combus_mainboard_ids.h`).
    The C++ enum identifiers (AnalogComBusID, etc.) are NOT prefixed —
    they remain stable across boards so application code does not need
    to change per board.
    """
    cap = _VIEW_ID_PREFIX[view.name]
    base = _file_base(view.name, filename_prefix)
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6.1")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("#pragma once")
    body.append("")
    body.append(f"#include \"{base}_ids.h\"")
    body.append("")
    body.append("#include <core/system/combus/combus_defs.h>")
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


def _render_source(view: View, ctx: BuildContext, *, filename_prefix: str = "") -> str:
    """
    Render the `<view>.cpp` file.

    Contains:
      - AnalogComBusArray[] (if analog channels exist)
      - DigitalComBusArray[] (if digital channels exist)
      - comBus definition (combus view only)

    `filename_prefix` (ND1): when non-empty, the `#include` directive
    targets the prefixed header (e.g. `combus_mainboard.h`).
    """
    cap = _VIEW_ID_PREFIX[view.name]
    base = _file_base(view.name, filename_prefix)
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]

    body: list[str] = []
    body.append(_HEADER_PROLOGUE
                .replace("<generator_version>", "A6.1")
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append(f"#include \"{base}.h\"")
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
            _emit_channel_init(body, vc, "analog", is_connected_board(ctx))
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
            _emit_channel_init(body, vc, "digital", is_connected_board(ctx))
        body.append("};")
        body.append("")

    if view.name == "combus":
        body.append("// =============================================================================")
        body.append("// 3. BUS INSTANCE")
        body.append("// =============================================================================")
        body.append("")
        body.append("ComBus comBus {")
        # RL3: runLevel + runLevelLayer removed from struct. RUNLEVEL is a
        #       plain analog channel; initialise via combus_set_analog() at
        #       runtime in machine_init().
        body.append(f"    .analogBus       = Analog{cap}Array,")
        body.append(f"    .digitalBus      = Digital{cap}Array,")
        body.append("    .analogBusMaxVal = (1UL << (sizeof(uint16_t) * 8)) - 1")
        body.append("};")
        body.append("")

    body.append("// EOF")
    return "\n".join(body) + "\n"


# =============================================================================
# LINK REWORK 2.4 — DIRECTION INSIDE THE NODE
# =============================================================================

# Flags that mean "this board establishes the connection with another node":
# it is the source, inside the node, of the BASE channels it receives.
INTER_NODE_FLAGS: frozenset[str] = frozenset({
    "INPUT_PS4_DS4_BT",   # the PS4 controller stands for the remote
    # Add the inter-node transports here (ESP-NOW, radio, ...).
})


def is_connected_board(ctx: BuildContext) -> bool:
    """True if the board of `ctx` establishes a connection with another node."""
    return bool(INTER_NODE_FLAGS & ctx.defines)


def node_direction(ch: ChannelDefinition, connected: bool) -> frozenset[str]:
    """Direction of a channel inside the node, for a board connected or not to another node.

    SYSTEM / BOARD: the direction itself (SYSTEM diffusion).  BASE: the .cb
    direction is the direction of the node; inside the node only
    `downlink_pt` circulates (the connected board redistributes it: uplink,
    the others receive it: downlink) and `both_or` (relayed by the 1.1b
    merge).  `downlink`, `uplink`, `both`, `none`: not in the node.
    """
    if ch.scope != "BASE":
        return ch.effective_direction
    if ch.direction_or == frozenset({"uplink", "downlink"}):
        return ch.effective_direction
    if ch.passthrough:
        return frozenset({"uplink"}) if connected else frozenset({"downlink"})
    return frozenset()


def check_single_connected_board(boards: dict) -> None:
    """Refuse a node with several boards connected to other nodes (confrontation in step 3.1).

    `boards` maps a board id to its BuildContext; the mother env (id None)
    is not a board and is ignored.
    """
    connected = sorted(bid for bid, ctx in boards.items() if bid is not None and is_connected_board(ctx))
    if len(connected) > 1:
        raise GeneratorError(
            f"several boards of the node are connected to other nodes ({', '.join(connected)}); "
            f"the confrontation of their BASE channels comes with step 3.1 of the link rework"
        )


def _emit_channel_init(body: list[str], vc: ViewChannel, type_: str, connected: bool = False) -> None:
    """
    Emit a single `AnalogComBus` / `DigitalComBus` initializer entry.

    Output format (matches runtime AnalogComBus/DigitalComBus struct):

        { .infoName = "...", .value = ..., .layer = ChanLayer::X,
          .flow = { .betweenNodes = Direction::Y, .betweenBoards = Direction::Z },
          .bothOr = { .enabled = false } },

    The `.value` field is rendered from the canonical channel value
    (see _render_value). It honours the YAML `value:` contract :
      - analog : CbusMinVal / CbusNeutral / CbusMaxVal / `<n>u`
      - digital : false / true
    Falls back to the generator default when `value:` is absent.

    The `.layer` field comes from the channel's scope (BASE/SYSTEM/BOARD).
    The `.flow` field (ChanFlow) holds the two directions: `.betweenNodes`
    (the bitmask from Direction.from_frozenset, used on an inter-node link)
    and `.betweenBoards` (the direction inside the node, node_direction(),
    used on an intra-node link, link rework 2.4).

    The `.bothOr` field (A2.1, a BothOrMerge) is ONLY emitted for digital
    channels.  Its `.enabled` is initialised to `true` iff the channel's `direction_or` is
    exactly `{uplink, downlink}` (i.e. the YAML token was `both_or`).
    For all other digital channels (uplink/downlink/both/none and
    uplink_or/downlink_or), it is `false`. For analog channels, the
    field is NOT emitted at all (the C++ struct has no `.bothOr` on
    AnalogComBus). The `bool bothOr` field in DigitalComBus
    (combus_defs.h) remains in place but is currently inert — the
    runtime wiring (combus_frame_apply) is deferred to a separate
    chantier.
    """
    escaped_info = _escape_cpp_string(vc.ch.info_name)
    layer = _SCOPE_TO_CHANLAYER[vc.ch.scope]
    dir_name = Direction.to_cpp_enum_name(vc.direction_bits)
    node_dir_name = Direction.to_cpp_enum_name(Direction.from_frozenset(node_direction(vc.ch, connected)))
    value_literal = _render_value(vc.ch, type_)
    if type_ == "digital":
        # A2.1 : .bothOr = true iff direction_or == {uplink, downlink}
        # (i.e. the YAML token was `both_or`). All other digital
        # tokens (uplink, downlink, both, none, uplink_or, downlink_or)
        # yield .bothOr = false.
        both_or_lit = "true" if vc.ch.direction_or == frozenset({"uplink", "downlink"}) else "false"
        body.append(
            f"    {{ .infoName = \"{escaped_info}\", "
            f".value = {value_literal}, "
            f".layer = {layer}, "
            f".flow = {{ .betweenNodes = {dir_name}, .betweenBoards = {node_dir_name} }}, "
            f".bothOr = {{ .enabled = {both_or_lit} }} }}, // {vc.ch.id}"
        )
    else:
        # analog: .bothOr does not exist (validation rejects
        # both_or=true on analog channels at the canon layer).
        body.append(
            f"    {{ .infoName = \"{escaped_info}\", "
            f".value = {value_literal}, "
            f".layer = {layer}, "
            f".flow = {{ .betweenNodes = {dir_name}, .betweenBoards = {node_dir_name} }} }}, // {vc.ch.id}"
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


# Views that should NOT emit the .h/.cpp runtime artifacts.
# Phase 2 / A12 hook: only the ids + md5 headers are written to disk.
# The .h/.cpp rendering is SKIPPED entirely for these views —
# _render_header() and _render_source() are never called. Only the
# ids header is rendered and written. The MD5 payload for these views
# is built independently, directly from the in-memory View model
# (see md5.canonical_bytes()), so no intermediate .cpp file is needed.
_NO_EMIT_RUNTIME_VIEWS = frozenset({"combus_system", "combus_base"})


def emit_view(
    view: View,
    out_dir: Path,
    ctx: BuildContext,
    *,
    filename_prefix: str = "",
) -> list[Path]:
    """
    Emit the ids header for a view into out_dir.

    Phase 2 / A12 hook: for combus_system and combus_base, the .h/.cpp
    runtime artifacts are NO LONGER written to disk. Only the
    <view>_ids.h header is emitted.

    The MD5 payload is built directly from the in-memory View model
    (see md5.canonical_bytes), so the .h/.cpp files are not needed as
    an intermediate for hashing.

    `filename_prefix` (ND1): when non-empty, every emitted file is
    renamed with the prefix inserted between the view name and the
    suffix (e.g. `combus_mainboard_ids.h`).

    Returns the list of written paths.
    """
    written: list[Path] = []
    base = _file_base(view.name, filename_prefix)
    # Always emit the ids header (this is the only artifact the rest
    # of the codebase includes via `#include "combus_system_ids.h"` etc.).
    _safe_write(out_dir / f"{base}_ids.h", _render_ids_header(view, ctx))
    written.append(out_dir / f"{base}_ids.h")
    # Skip the .h/.cpp emission for views that don't need them.
    if view.name in _NO_EMIT_RUNTIME_VIEWS:
        return written
    # For combus (full view), emit the .h/.cpp as before.
    written.append(_safe_write_collect(out_dir / f"{base}.h", _render_header(view, ctx, filename_prefix=filename_prefix)))
    written.append(_safe_write_collect(out_dir / f"{base}.cpp", _render_source(view, ctx, filename_prefix=filename_prefix)))
    return written


def _safe_write_collect(path: Path, content: str) -> Path:
    """Write content atomically and return the path (helper for emit_view)."""
    _safe_write(path, content)
    return path


def _emit_md5_files(
    sel: ViewSelection,
    out_dir: Path,
    filename_prefix: str,
    project_version: tuple[int, int, int] | None = None,
) -> dict:
    """
    Return the hashes (view name -> ViewHash) of the views.

    Write the MD5 headers, with the ND1 prefix inserted in the three
    view-specific names.

    ND1 deliberately avoids touching md5.py, which always writes the
    un-prefixed names. With a prefix, md5.py therefore writes into a
    scratch subdirectory and the files are moved into out_dir under
    their prefixed names. Writing the un-prefixed names directly in
    out_dir and renaming them would delete the un-prefixed MD5 headers
    of another board generated earlier into the same out_dir (node
    mode: the board being compiled owns the un-prefixed names).
    combus_wire_common.h is NOT renamed (it is a shared header).
    """
    from .md5 import generate_md5_artifacts
    version = {}
    if project_version is not None:
        version = {
            "project_version_major": project_version[0],
            "project_version_minor": project_version[1],
            "project_version_dev": project_version[2],
        }
    if not filename_prefix:
        hashes, _ = generate_md5_artifacts(sel, out_dir, **version)
        return hashes
    scratch = out_dir / "_md5_scratch"
    hashes, _ = generate_md5_artifacts(sel, scratch, **version)
    for src in scratch.iterdir():
        name = src.name
        for view_name in ("combus", "combus_system", "combus_base"):
            if name == f"{view_name}_md5.h":
                name = f"{view_name}_{filename_prefix}_md5.h"
                break
        src.replace(out_dir / name)
    scratch.rmdir()
    return hashes


def _print_set_hashes(sel: ViewSelection, hashes: dict, filename_prefix: str) -> None:
    """Chantier 2.1 — one build-log line per channel set: its MD5 and its size."""
    board = f"  [{filename_prefix}]" if filename_prefix else ""
    from .md5 import stable_view

    n_candidates = sum(1 for vc in sel.base.channels if vc.ch.candidate)
    for label, hash_name, view, extra in (
            ("BASE", "combus_base", stable_view(sel.base), ""),
            ("NEXT", "combus_base_next", sel.base,
             f"  ({n_candidates} candidate{'s' if n_candidates != 1 else ''})"),
            ("SYSTEM", "combus_system", sel.system, "")):
        n_analog = sum(1 for vc in view.channels if vc.ch.type == "analog")
        n_digital = sum(1 for vc in view.channels if vc.ch.type == "digital")
        print(f"[combus_builder] set {label:<6}  md5={hashes[hash_name].md5_hex}  "
              f"A{n_analog}+D{n_digital}{extra}{board}")


def generate(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
    out_dir: Path,
    *,
    filename_prefix: str = "",
    project_version: tuple[int, int, int] | None = None,
) -> ViewSelection:
    """
    Full A7 + A12 generation pipeline.

    1. Resolve `requires` against ctx.
    2. Select views (combus, combus_system, combus_base).
    3. Emit the view headers into out_dir.
       - combus: 3 files (ids.h, .h, .cpp)
       - combus_system, combus_base: 1 file each (ids.h only — Phase 2)
       The .h/.cpp files for combus_system and combus_base are NOT
       written to disk anymore (Phase 2): the MD5 payload is built
       from the in-memory View model, so no intermediate file is
       needed. See emit_view() and _NO_EMIT_RUNTIME_VIEWS.
    4. Emit MD5 artifacts (combus_md5.h, combus_system_md5.h,
       combus_base_md5.h, combus_wire_common.h).

    `project_version`: optional (major, minor, dev) written in
    combus_wire_common.h (see md5.read_project_version()).

    `filename_prefix` (ND1): when non-empty, every emitted file is
    renamed with the prefix inserted between the view name and the
    suffix (e.g. `combus_mainboard_ids.h`). The C++ enum identifiers
    are NOT prefixed — they remain stable across boards. No selection /
    sort / numbering logic is touched.

    Returns the ViewSelection so callers (tests, diagnostics) can
    inspect what was generated.
    """
    skipped: list[tuple[str, list[str]]] = []
    active = resolve_requires(channels, ctx, skipped)
    for channel_id, missing in skipped:
        print(f"[combus_builder] requires: channel '{channel_id}' not generated "
              f"(flag not defined: {', '.join(missing)})")
    sel = select_views(active)
    emit_view(sel.full, out_dir, ctx, filename_prefix=filename_prefix)
    emit_view(sel.system, out_dir, ctx, filename_prefix=filename_prefix)
    emit_view(sel.base, out_dir, ctx, filename_prefix=filename_prefix)
    # projectVersion: (major, minor, dev) read from project_version.h by the
    # caller; None keeps the md5.py placeholder (tests, ad-hoc calls).
    hashes = _emit_md5_files(sel, out_dir, filename_prefix, project_version)
    _print_set_hashes(sel, hashes, filename_prefix)
    _emit_peer_links(active, ctx, out_dir, filename_prefix)
    return sel


def _emit_peer_links(channels: list[ChannelDefinition], ctx: BuildContext, out_dir: Path,
                     filename_prefix: str) -> None:
    """Link rework 3.1a — the links with the peer types of this node (combus_peer_links.h).

    Always written (empty without peer), so that the build has a fixed set of
    artefacts.  One build-log line per link: its MD5 and its size.
    """
    from .links import LinkError, compute_link_set, link_md5, load_type_base_channels, \
        peer_types, render_peer_links_header
    from .machine_type import detect_machine_type

    links = []
    peers = peer_types(ctx)
    if peers:
        local_type = detect_machine_type(ctx)
        if local_type is None:
            raise GeneratorError(f"peer types {peers} declared, but no MACHINE_TYPE_* flag for this node")
        try:
            for peer in peers:
                link = compute_link_set(local_type, channels, peer,
                                        load_type_base_channels(ctx.buildroot, peer))
                for warning in link.warnings:
                    print(f"[combus_builder] link {peer}: {warning}")
                n_analog = sum(1 for c in link.channels if c.type == "analog")
                n_digital = sum(1 for c in link.channels if c.type == "digital")
                board = f"  [{filename_prefix}]" if filename_prefix else ""
                print(f"[combus_builder] set LINK {peer}  md5={link_md5(link)}  A{n_analog}+D{n_digital}{board}")
                links.append(link)
        except LinkError as e:
            raise GeneratorError(f"link with a peer type: {e}") from e

    ids_header = f"{_file_base('combus', filename_prefix)}_ids.h"
    _safe_write(out_dir / f"{_file_base('combus_peer_links', filename_prefix)}.h",
                render_peer_links_header(links, ids_header))


# =============================================================================
# CLI / diagnostic
# =============================================================================

def main(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
    out_dir: Path,
    *,
    filename_prefix: str = "",
) -> int:
    """CLI entry point: generate artifacts and print a summary."""
    try:
        sel = generate(channels, ctx, out_dir, filename_prefix=filename_prefix)
    except GeneratorError as e:
        sys.stderr.write(f"[combus_builder] FATAL: {e}\n")
        return 1

    base_full = _file_base("combus", filename_prefix)
    file_system = _file_base("combus_system", filename_prefix)
    file_base = _file_base("combus_base", filename_prefix)
    print(f"[combus_builder] generated view 'combus' "
          f"({sel.full.ch_count} channels, WIRE_END={sel.full.wire_end}) "
          f"-> {out_dir}/{base_full}.{{h,cpp,_ids.h}}")
    print(f"[combus_builder] generated view 'combus_system' "
          f"({sel.system.ch_count} channels, WIRE_END={sel.system.wire_end}) "
          f"-> {out_dir}/{file_system}.{{h,cpp,_ids.h}}")
    print(f"[combus_builder] generated view 'combus_base' "
          f"({sel.base.ch_count} channels, WIRE_END={sel.base.wire_end}) "
          f"-> {out_dir}/{file_base}.{{h,cpp,_ids.h}}")
    return 0
