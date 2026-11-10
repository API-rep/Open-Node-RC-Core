#!/usr/bin/env python3
"""
combus_builder/generator.py - C++ artifact generation.

What it does:
  - Take the canonised ChannelDefinitions (canon/) and the BuildContext
    (flags.py).
  - Resolve `requires` against the BuildContext.
  - Allocate deterministic numeric IDs per view.
  - Emit the runtime structures of src/core/system/combus/combus_defs.h
    (AnalogComBus / DigitalComBus: infoName, value, layer, flow, plus
    zeroAtNeutral or bothOr), not an abstraction of them.
  - Three views: combus (BASE + SYSTEM + BOARD), combus_system
    (BASE + SYSTEM) and combus_base (BASE only).

Not here:
  - MD5 (md5.py) and the links between nodes (links.py).
  - `chains:` (TODO(ComBus processors)).

Direction representation (C++):
  - `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2, Both=3 }`
  - Bit layout: bit 0 = uplink, bit 1 = downlink.
  - Both = static_cast<Direction>(Uplink|Downlink) = 3.
  - No bitwise operators are defined on Direction in the C++ code.

View contract:
  - combus (BASE+SYSTEM+BOARD):
      * per type, ID range [0, CH_COUNT) where BASE is [0, WIRE_END) and
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

Node mode (several boards):
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
    """Base class of the generator errors."""


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
    ch:         the original channel definition (canon/channels.py).
    direction_bits: 0/1/2/3 from Direction.from_frozenset.
    """

    numeric_id: int
    ch: ChannelDefinition
    direction_bits: int


@dataclass(frozen=True)
class View:
    """
    One resolved view (combus, combus_system or combus_base).

    name:             "combus", "combus_system" or "combus_base".
    channels:         list of ViewChannel in ID order.
    wire_end:         count of BASE channels, both types (== len if no SYSTEM/BOARD).
    wire_end_analog:  count of BASE analog channels: the wire end of the
                      analog bus (analog and digital wire ends are NOT
                      always equal).
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
    system: combus_system (BASE + SYSTEM)
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
# Filename prefix of a board (node mode)
# =============================================================================

def _file_base(view_name: str, filename_prefix: str) -> str:
    """
    Return the file basename for a view, with the board prefix applied.

    Single board (filename_prefix == ""): returns view_name unchanged.
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
# BASE channels occupy the indices [0..WIRE_END) of their type: the frame
# carries this prefix only.
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

    ID allocation policy:
        Each TYPE (analog, digital) has its OWN 0-based counter. The
        numeric_id assigned to a channel is its position within its type
        in the view, NOT its position in the global view-wide sequence.
        This guarantees that `DigitalComBusArray[DigitalComBusID::XXX]`
        accesses the correct slot: one counter for both types would make
        the digital IDs start after the analog ones, out of the array.

    allowed_scopes is the set of scopes that belong to this view.
    For combus: {"BASE", "SYSTEM", "BOARD"}.
    For combus_base: {"BASE"}.

    Sort key (stable, per-type):
        (scope priority, candidate, theme, id)
    Scope priority: BASE < SYSTEM < BOARD (preserves wire order:
    BASE channels are at the lowest indices, SYSTEM next, BOARD last).
    The BASE candidates come after the stable BASE
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
    # Per-bus wire-end counters (analog vs digital).  Both counters track BASE channels only, separated by type.  The
    # final values (after the loop) represent the count of BASE
    # channels of each type in the view, which may differ from
    # `wire_end` (= total BASE count across both types).
    wire_end_analog = 0
    wire_end_digital = 0
    # Separate 0-based counters per type.
    analog_idx = 0
    digital_idx = 0
    # The wire order is: all analog channels first (in scope/theme/id
    # order), then all digital channels (in scope/theme/id order).  Within
    # each type the BASE channels hold the lowest indices, because they
    # sort before SYSTEM/BOARD.
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

    The combus_system view is a strict prefix of combus: it
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
 * Generated by scripts/combus_builder/generator.py.
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
    channel WHEN THE YAML CHANNEL DEFINES NO `value:` (parser.py refuses
    that in a `.cb`; the default serves the direct calls and the tests).

    Analog defaults to CbusNeutral (neutral 16-bit value). Digital defaults
    to false. The runtime may override these at boot via combus_set_*().

    Note : when the YAML declares an explicit `value`, the canonical
    representation in ChannelDefinition.value is used instead — see
    `_render_value`.
    """
    if type_ == "analog":
        return "CbusNeutral"
    return "false"


# Canon token → C++ literal, for the accepted `value:` tokens.  The analog
# tokens are emitted as is (they are C++ symbols); the digital ones become
# boolean literals.
_VALUE_TOKEN_TO_CPP: dict[str, str] = {
    # analog
    "CbusMinVal":  "CbusMinVal",
    "CbusNeutral": "CbusNeutral",
    "CbusMaxVal":  "CbusMaxVal",
    # digital
    "low":  "false",
    "high": "true",
    "fault": "true",   # safe value of a failsafe alarm, same literal as `high`
}


def _render_value(ch: ChannelDefinition, type_: str) -> str:
    """
    Return the C++ literal for the `value` field of a freshly-generated
    channel.

    Resolution order :
      1. If `ch.value is None` (YAML absent) → use the generator default
         (analog: CbusNeutral, digital: false).
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


# Mapping view.name -> C++ identifier prefix.  The C++ code writes "ComBus"
# (capital B): str.capitalize() would yield "Combus", so the form is written
# here.
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
                .replace("<view_name>", view.name)
                .replace("<ctx_summary>", _ctx_summary(ctx)))
    body.append("")
    body.append("#pragma once")
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

    # No wire-end constant: the wire dimensions are the CH_COUNT of the BASE
    # view enums (AnalogComBusBaseID, DigitalComBusBaseID), at the call site.

    body.append("// EOF")
    return "\n".join(body) + "\n"


def _render_header(view: View, ctx: BuildContext, *, filename_prefix: str = "") -> str:
    """
    Render the `<view>.h` file.

    Contains:
      - Forward declaration / inclusion of the IDs header
      - extern declarations of the channel arrays
      - extern declaration of the bus instance (combus view only)

    `filename_prefix` (node mode): when non-empty, the `#include` directive
    targets the prefixed ids header (e.g. `combus_mainboard_ids.h`).
    The C++ enum identifiers (AnalogComBusID, etc.) are NOT prefixed —
    they remain stable across boards so application code does not need
    to change per board.
    """
    cap = _VIEW_ID_PREFIX[view.name]
    base = _file_base(view.name, filename_prefix)
    body: list[str] = []
    body.append(_HEADER_PROLOGUE
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

    `filename_prefix` (node mode): when non-empty, the `#include` directive
    targets the prefixed header (e.g. `combus_mainboard.h`).
    """
    cap = _VIEW_ID_PREFIX[view.name]
    base = _file_base(view.name, filename_prefix)
    analog = [vc for vc in view.channels if vc.ch.type == "analog"]
    digital = [vc for vc in view.channels if vc.ch.type == "digital"]

    body: list[str] = []
    body.append(_HEADER_PROLOGUE
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
        # The run level is the plain analog channel RUNLEVEL, not a field of
        # the struct.
        body.append(f"    .analogBus       = Analog{cap}Array,")
        body.append(f"    .digitalBus      = Digital{cap}Array,")
        body.append("    .analogBusMaxVal = (1UL << (sizeof(uint16_t) * 8)) - 1")
        body.append("};")
        body.append("")

    body.append("// EOF")
    return "\n".join(body) + "\n"


# =============================================================================
# DIRECTION INSIDE THE NODE
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
    the others receive it: downlink) and `both_or` (relayed by the OR
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
    """Refuse a node with several boards connected to other nodes.

    `boards` maps a board id to its BuildContext; the mother env (id None)
    is not a board and is ignored.
    """
    connected = sorted(bid for bid, ctx in boards.items() if bid is not None and is_connected_board(ctx))
    if len(connected) > 1:
        raise GeneratorError(
            f"several boards of the node are connected to other nodes ({', '.join(connected)}); "
            f"only one board of a node can be connected to other nodes"
        )


def _emit_channel_init(body: list[str], vc: ViewChannel, type_: str, connected: bool = False) -> None:
    """
    Emit a single `AnalogComBus` / `DigitalComBus` initializer entry.

    Output format (matches runtime AnalogComBus/DigitalComBus struct):

        { .infoName = "...", .value = ..., .layer = ChanLayer::X,
          .flow = { .betweenNodes = Direction::Y, .betweenBoards = Direction::Z },
          .bothOr = { .enabled = false } },

    The `.value` field is rendered from the canonical channel value
    (see _render_value). It honours the YAML `value:` contract:
      - analog: CbusMinVal / CbusNeutral / CbusMaxVal / `<n>u`
      - digital: false / true (`high` and `fault` are true)
    Falls back to the generator default when `value:` is absent.

    The `.layer` field comes from the channel's scope (BASE/SYSTEM/BOARD).
    The `.flow` field (ChanFlow) holds the two directions: `.betweenNodes`
    (the bitmask from Direction.from_frozenset, used on an inter-node link)
    and `.betweenBoards` (the direction inside the node, node_direction(),
    used on an intra-node link).

    The `.bothOr` field (a BothOrMerge) is ONLY emitted for digital
    channels.  Its `.enabled` is `true` iff the channel's `direction_or` is
    exactly `{uplink, downlink}` (i.e. the YAML token was `both_or`), and
    `false` for every other digital channel.  For analog channels the
    field is NOT emitted (AnalogComBus has no `.bothOr`); `.zeroAtNeutral`
    is emitted instead, for a `zeroAt: neutral` channel only.
    """
    escaped_info = _escape_cpp_string(vc.ch.info_name)
    layer = _SCOPE_TO_CHANLAYER[vc.ch.scope]
    dir_name = Direction.to_cpp_enum_name(vc.direction_bits)
    node_dir_name = Direction.to_cpp_enum_name(Direction.from_frozenset(node_direction(vc.ch, connected)))
    value_literal = _render_value(vc.ch, type_)
    if type_ == "digital":
        # .bothOr = true iff direction_or == {uplink, downlink}
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
        # `.zeroAtNeutral = true` only for a `zeroAt: neutral`
        # channel; the entry of every other channel is unchanged.
        zero_at = ", .zeroAtNeutral = true" if vc.ch.zero_at_neutral else ""
        body.append(
            f"    {{ .infoName = \"{escaped_info}\", "
            f".value = {value_literal}, "
            f".layer = {layer}, "
            f".flow = {{ .betweenNodes = {dir_name}, .betweenBoards = {node_dir_name} }}{zero_at} }}, // {vc.ch.id}"
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


# Views that should NOT emit the .h/.cpp runtime artifacts: only the ids
# and md5 headers are written to disk.  The .h/.cpp rendering is SKIPPED entirely for these views —
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

    For combus_system and combus_base, the .h/.cpp runtime artifacts are
    NOT written to disk: only the <view>_ids.h header is emitted.

    The MD5 payload is built directly from the in-memory View model
    (see md5.canonical_bytes), so the .h/.cpp files are not needed as
    an intermediate for hashing.

    `filename_prefix` (node mode): when non-empty, every emitted file is
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
    # For combus (full view), emit the .h/.cpp too.
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

    Write the MD5 headers, with the board prefix inserted in the three
    view-specific names.

    md5.py always writes the un-prefixed names.  With a prefix, md5.py therefore writes into a
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
    """One build-log line per channel set: its MD5 and its size."""
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
    Full generation pipeline.

    1. Add the contract of the link with the remote, mirror the view of a
       remote, inject the channels of the couplers.
    2. Resolve `requires` against ctx (each skipped channel is logged).
    3. Select views (combus, combus_system, combus_base).
    4. Emit the view headers into out_dir.
       - combus: 3 files (ids.h, .h, .cpp)
       - combus_system, combus_base: 1 file each (ids.h only)
       The MD5 payload is built from the in-memory View model, so no
       intermediate file is needed. See emit_view() and
       _NO_EMIT_RUNTIME_VIEWS.
    5. Emit MD5 artifacts (combus_md5.h, combus_system_md5.h,
       combus_base_md5.h, combus_wire_common.h) and log the set hashes.
    6. Emit combus_peer_links.h (the links between nodes).

    `project_version`: optional (major, minor, dev) written in
    combus_wire_common.h (see md5.read_project_version()).

    `filename_prefix` (node mode): when non-empty, every emitted file is
    renamed with the prefix inserted between the view name and the
    suffix (e.g. `combus_mainboard_ids.h`). The C++ enum identifiers
    are NOT prefixed — they remain stable across boards. No selection /
    sort / numbering logic is touched.

    Returns the ViewSelection so callers (tests, diagnostics) can
    inspect what was generated.
    """
    contract = _remote_contract(channels, ctx)
    if contract:                            # the commands of the supported couplers are in the contract
        known = {ch.id for ch in channels}
        channels = list(channels) + [c for c in contract[1] if c.id not in known]
    channels = _remote_view(channels, ctx)
    channels, couplers = _inject_couplers(channels, ctx)
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
    _emit_peer_links(active, couplers, contract, ctx, out_dir, filename_prefix)
    return sel


def _remote_contract(channels: list[ChannelDefinition], ctx: BuildContext):
    """(machine type, commands) of the link with the remote, or None.

    None for a build without category or machine type.  Checks the couplers
    the type supports (.mtype): one meaning per name, one source per channel.
    """
    from . import links as links_module
    from .links import NODE_CATEGORIES, LinkError, check_supported_couplers, remote_commands
    from .machine_type import detect_machine_type

    defines = set(ctx.defines) | set(ctx.defines_with_value or {})
    machine_type = detect_machine_type(ctx)
    if machine_type is None or not any(c in defines for c in NODE_CATEGORIES):
        return None
    try:
        config = links_module.load_machine_type_config(ctx.buildroot, machine_type)
        supported = {name: links_module.load_coupler_channels(ctx.buildroot, name) for name in config.socket}
        type_base = [ch for ch in channels if ch.scope == "BASE"]
        check_supported_couplers(type_base, supported)
    except LinkError as e:
        raise GeneratorError(f"contract of the machine type {machine_type!r}: {e}") from e
    plugs = {name: links_module.load_coupler_channels(ctx.buildroot, name) for name in config.plug}
    return machine_type, remote_commands(type_base, supported, plugs)


def _is_remote(ctx: BuildContext) -> bool:
    return "IS_REMOTE" in (set(ctx.defines) | set(ctx.defines_with_value or {}))


def _remote_view(channels: list[ChannelDefinition], ctx: BuildContext) -> list[ChannelDefinition]:
    """A remote (IS_REMOTE): the BASE view of the machine type it drives, mirrored.

    GeneratorError when the remote names no machine type.  Unchanged for a
    machine.
    """
    from .links import remote_channels
    from .machine_type import detect_machine_type

    if not _is_remote(ctx):
        return channels
    machine_type = detect_machine_type(ctx)
    if machine_type is None:
        raise GeneratorError("a remote drives a machine type: set -D MACHINE_TYPE_<TYPE>")
    return remote_channels(channels, machine_type)


def _inject_couplers(channels: list[ChannelDefinition], ctx: BuildContext):
    """The ComBus of the couplers merges into the ComBus of the node.

    Returns the channels with the channels of the declared couplers injected
    (links.inject_coupler_channels), and the couplers as {name: (side,
    channels)}; {} when the node declares no coupler.  A node that declares a
    coupler, or the removed COMBUS_PEER_<TYPE>, has its category and sides
    checked: a wrong env is refused explicitly, never ignored.
    """
    from . import links as links_module
    from .links import PLUG, PLUG_FLAG_PREFIX, REMOVED_PEER_FLAG_PREFIX, SOCKET, SOCKET_FLAG_PREFIX, LinkError, \
        coupler_sides, inject_coupler_channels, is_passive_coupler, node_category
    from .machine_type import detect_machine_type

    defines = set(ctx.defines) | set(ctx.defines_with_value or {})
    if not any(d.startswith((SOCKET_FLAG_PREFIX, PLUG_FLAG_PREFIX, REMOVED_PEER_FLAG_PREFIX)) for d in defines):
        return channels, {}
    try:
        node_category(defines)
        machine_type = detect_machine_type(ctx)
        sides = coupler_sides(defines)
        if sides and machine_type is None:
            raise LinkError("a coupler needs a machine type that supports it: set -D MACHINE_TYPE_<TYPE>")
        couplers = {}
        for name, side in sides.items():
            coupler_chs = links_module.load_coupler_channels(ctx.buildroot, name)
            if side == PLUG and is_passive_coupler(coupler_chs, name):
                raise LinkError(f"coupler {name!r} is passive (no link channel): it has a socket only, "
                                f"never a plug")
            config = links_module.load_machine_type_config(ctx.buildroot, machine_type)
            if name not in (config.socket if side == SOCKET else config.plug):
                raise LinkError(f"coupler {name!r}: its {side} is not supported by the machine type "
                                f"{machine_type!r} (declare it in {machine_type}.mtype)")
            couplers[name] = (side, coupler_chs)
            channels = inject_coupler_channels(channels, coupler_chs, name, side)
    except LinkError as e:
        raise GeneratorError(f"link through a coupler: {e}") from e
    return channels, couplers


def _emit_peer_links(channels: list[ChannelDefinition], couplers, contract, ctx: BuildContext, out_dir: Path,
                     filename_prefix: str) -> None:
    """The links through the couplers of this node (combus_peer_links.h).

    Always written (empty without coupler), so that the build has a fixed set
    of artefacts.  One build-log line per link: its MD5 and its size.
    """
    from .links import PLUG, SOCKET, LinkError, check_single_source, compute_coupler_link, compute_remote_link, \
        is_passive_coupler, link_md5, render_peer_links_header

    links = []
    try:
        for name, (side, coupler_chs) in couplers.items():
            if is_passive_coupler(coupler_chs, name):     # a local instance, no link
                print(f"[combus_builder] set COUPLER {name} (passive)")
                continue
            link = compute_coupler_link(name, coupler_chs, channels, side)
            n_analog = sum(1 for c in link.channels if c.type == "analog")
            n_digital = sum(1 for c in link.channels if c.type == "digital")
            board = f"  [{filename_prefix}]" if filename_prefix else ""
            print(f"[combus_builder] set LINK {name} ({link.role})  md5={link_md5(link)}  "
                  f"A{n_analog}+D{n_digital}{board}")
            links.append(link)
        if contract and contract[1]:        # the link with the remote, its own contract
            machine_type, commands = contract
            link = compute_remote_link(machine_type, commands, PLUG if _is_remote(ctx) else SOCKET)
            print(f"[combus_builder] set LINK {link.coupler} ({link.role})  md5={link_md5(link)}  "
                  f"A{sum(1 for c in link.channels if c.type == 'analog')}"
                  f"+D{sum(1 for c in link.channels if c.type == 'digital')}")
            links.append(link)
        check_single_source(links)          # one source per channel on the node
    except LinkError as e:
        raise GeneratorError(f"link through a coupler: {e}") from e

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
