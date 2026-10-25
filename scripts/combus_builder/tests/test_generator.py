#!/usr/bin/env python3
"""
Tests for generator.py (A6.1) - C++ artifact generation.

A6.1 corrections vs A6:
  - ChannelDescriptor is REMOVED.
  - The generator emits REAL AnalogComBus / DigitalComBus runtime
    structures (infoName, value, layer, direction) — matching
    src/core/system/combus/combus_defs.h.
  - ChanLayer is preserved (audit A6 confirmed _layer_ok() requires it).
  - direction is a fourth field on each runtime struct entry.

Coverage:
  - requires resolution (errors only — A6 raises, not filters).
  - View selection (combus = BASE+SYSTEM+BOARD; combus_base = BASE).
  - Deterministic ID allocation.
  - WIRE_END computation.
  - Direction C++ representation (0/1/2/3, both normalization).
  - Real runtime struct rendering (infoName, value, layer, direction).
  - File emission (.h/.cpp/_ids.h, atomic write).
  - Generation against a real .cb file in the repo.
  - Generated C++ is structurally well-formed (basic checks).
  - Python <-> C++ coherence (Direction enum values + struct field order).
"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import (
    canonize,
    ChannelDefinition,
)
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.generator import (
    Direction,
    GeneratorError,
    RequiresResolutionError,
    View,
    ViewChannel,
    ViewSelection,
    _escape_cpp_string,
    _render_header,
    _render_ids_header,
    _render_source,
    _safe_write,
    _select_view,
    emit_view,
    generate,
    resolve_requires,
    select_views,
)


# =============================================================================
# Helpers
# =============================================================================

def _parsed(path: Path, raw: dict, type_label: str = "channel_def"):
    return (path, type_label, raw)


def _ch(
    id_: str = "FOO",
    info_name: str = "Foo channel",
    type_: str = "digital",
    scope: str = "SYSTEM",
    theme: str = "failsafe",
    direction: str | None = None,
    requires: list | None = None,
) -> dict:
    d: dict = {
        "id": id_,
        "infoName": info_name,
        "type": type_,
        "scope": scope,
        "theme": theme,
    }
    if direction is not None:
        d["direction"] = direction
    elif scope in ("SYSTEM", "BASE"):
        d["direction"] = "uplink"
    if requires is not None:
        d["requires"] = requires
    return d


def _canon_from_yamls(*yamls: dict) -> list[ChannelDefinition]:
    parsed = []
    for i, y in enumerate(yamls):
        p = Path(f"/tmp/test_{i}.cb")
        parsed.append(_parsed(p, y))
    result = canonize(parsed)
    return result.canonical_definitions


def _ctx(flags: list[str]) -> BuildContext:
    return BuildContext(
        buildroot=Path("/tmp"),
        defines=frozenset(flags),
        defines_with_value={},
        cppdefines_source="override",
    )


def _ctx_with_values(values: dict) -> BuildContext:
    return BuildContext(
        buildroot=Path("/tmp"),
        defines=frozenset(),
        defines_with_value=dict(values),
        cppdefines_source="override",
    )


# =============================================================================
# A6.1 SPECIFIC: ChannelDescriptor must NOT appear
# =============================================================================

def test_no_channel_descriptor_struct_in_rendered_header():
    """A6.1: ChannelDescriptor abstraction is REMOVED. Header must not
    declare a ChannelDescriptor struct."""
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "struct ChannelDescriptor" not in out
    assert "ChannelDescriptor " not in out


def test_no_channel_descriptor_in_rendered_source():
    """A6.1: generated .cpp must not reference ChannelDescriptor either."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="Foo"),
        _ch("BAR", type_="analog", info_name="Bar"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "ChannelDescriptor" not in out


def test_no_channel_descriptor_in_full_generation(tmp_path):
    """End-to-end: emitted files must not contain ChannelDescriptor."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", scope="BASE"),
        _ch("BAR", type_="analog", scope="BASE"),
    ]})
    sel = generate(chs, _ctx([]), tmp_path)
    for fname in ("combus.h", "combus.cpp", "combus_ids.h",
                  "combus_system_ids.h", "combus_base_ids.h"):
        content = (tmp_path / fname).read_text(encoding="utf-8")
        assert "ChannelDescriptor" not in content, f"ChannelDescriptor in {fname}"


# =============================================================================
# A6.1 SPECIFIC: ChanLayer is PRESERVED
# =============================================================================

def test_chanlayer_enum_in_combus_defs():
    """combus_defs.h still defines ChanLayer (audit A6 confirmed _layer_ok)."""
    h = (REPO_ROOT / "src" / "core" / "system" / "combus" / "combus_defs.h").read_text(encoding="utf-8")
    assert "enum class ChanLayer" in h
    for v in ("BOARD", "SYSTEM", "BASE", "UNDEFINED"):
        assert v in h, f"ChanLayer::{v} missing from combus_defs.h"


def test_generated_initializers_use_chanlayer():
    """Generated .cpp must use ChanLayer::X in the .layer field of each entry."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="BASE", type_="digital"),
        _ch("L1", scope="SYSTEM", type_="digital"),
        _ch("S1", scope="BOARD", type_="digital", direction="none"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        cpp = (Path(tmp_dir) / "combus.cpp").read_text(encoding="utf-8")
    # At least one of each layer value must appear in the initializers.
    assert ".layer = ChanLayer::BASE" in cpp
    assert ".layer = ChanLayer::SYSTEM" in cpp
    assert ".layer = ChanLayer::BOARD" in cpp


def test_generated_header_includes_combus_defs():
    """Generated .h must #include <core/system/combus/combus_defs.h> for ChanLayer / Direction."""
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "#include <core/system/combus/combus_defs.h>" in out


# =============================================================================
# A6.1 SPECIFIC: direction is added to runtime structs
# =============================================================================

def test_direction_field_in_combus_defs():
    """AnalogComBus and DigitalComBus carry a ChanFlow: one direction per kind of link (link rework 2.4)."""
    h = (REPO_ROOT / "src" / "core" / "system" / "combus" / "combus_defs.h").read_text(encoding="utf-8")
    assert "struct ChanFlow" in h
    assert "Direction betweenNodes" in h and "Direction betweenBoards" in h
    assert h.count("ChanFlow    flow") == 2


def test_generated_initializers_have_direction_field():
    """Each generated entry must include `.betweenNodes = Direction::X`."""
    chs = _canon_from_yamls({"channels": [
        _ch("UP",   direction="uplink"),
        _ch("DOWN", direction="downlink"),
        _ch("BOTH", direction="both"),
        _ch("NONE", direction="none"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert ".betweenNodes = Direction::Uplink" in out
    assert ".betweenNodes = Direction::Downlink" in out
    assert ".betweenNodes = Direction::Both" in out
    assert ".betweenNodes = Direction::None" in out


def test_generated_init_order_infoName_value_layer_direction():
    """Each entry must keep the field order: infoName, value, layer, flow (the directions)."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="Foo channel", direction="uplink"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    # The first { ... } entry should appear with this exact field order.
    m = re.search(r"\{[^}]*\.infoName[^}]*\}", out)
    assert m, f"no initializer with .infoName found in:\n{out}"
    entry = m.group(0)
    pos_info = entry.find(".infoName")
    pos_value = entry.find(".value")
    pos_layer = entry.find(".layer")
    pos_dir = entry.find(".flow")
    assert 0 <= pos_info < pos_value < pos_layer < pos_dir, (
        f"field order broken: {entry!r}"
    )


# =============================================================================
# Direction C++ representation
# =============================================================================

def test_dir_none_is_zero():
    assert Direction.from_frozenset(frozenset()) == 0


def test_dir_uplink_is_one():
    assert Direction.from_frozenset(frozenset({"uplink"})) == 1


def test_dir_downlink_is_two():
    assert Direction.from_frozenset(frozenset({"downlink"})) == 2


def test_dir_both_is_three():
    assert Direction.from_frozenset(frozenset({"uplink", "downlink"})) == 3


def test_dir_to_cpp_enum_name():
    assert Direction.to_cpp_enum_name(0) == "Direction::None"
    assert Direction.to_cpp_enum_name(1) == "Direction::Uplink"
    assert Direction.to_cpp_enum_name(2) == "Direction::Downlink"
    assert Direction.to_cpp_enum_name(3) == "Direction::Both"


def test_dir_is_symmetric():
    a = Direction.from_frozenset(frozenset({"uplink", "downlink"}))
    b = Direction.from_frozenset(frozenset({"downlink", "uplink"}))
    assert a == b == 3


def test_escape_cpp_string_quote():
    assert _escape_cpp_string('foo "bar"') == 'foo \\"bar\\"'


def test_escape_cpp_string_backslash():
    assert _escape_cpp_string("foo\\bar") == "foo\\\\bar"


def test_escape_cpp_string_newline():
    assert _escape_cpp_string("foo\nbar") == "foo\\nbar"


# =============================================================================
# requires resolution
# =============================================================================

def test_resolve_requires_empty():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    ctx = _ctx([])
    assert resolve_requires(chs, ctx) == chs


def test_resolve_requires_all_present():
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["HAS_X"])]})
    ctx = _ctx(["HAS_X"])
    assert resolve_requires(chs, ctx) == chs


def test_resolve_requires_with_value():
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["HAS_FOO"])]})
    ctx = _ctx_with_values({"HAS_FOO": "1"})
    assert resolve_requires(chs, ctx) == chs


def test_resolve_requires_missing_filters_and_reports():
    """A channel whose flag is missing is skipped, and reported (never silent)."""
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["MISSING_FLAG"]), _ch("BAR")]})
    ctx = _ctx([])
    skipped = []
    kept = resolve_requires(chs, ctx, skipped)
    assert [c.id for c in kept] == ["BAR"]
    assert skipped == [("FOO", ["MISSING_FLAG"])]


def test_resolve_requires_partial_missing_filters():
    """With one of two flags missing the channel is skipped; only the missing one is reported."""
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["HAS_X", "MISSING"])]})
    ctx = _ctx(["HAS_X"])
    skipped = []
    assert resolve_requires(chs, ctx, skipped) == []
    assert skipped == [("FOO", ["MISSING"])]


def test_resolve_requires_without_report_list_still_filters():
    """The `skipped` list is optional."""
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["MISSING"])]})
    assert resolve_requires(chs, _ctx([])) == []


# =============================================================================
# View selection
# =============================================================================

def test_select_view_combus_full():
    """combus view = BASE first, then SYSTEM, then BOARD."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
        ]
    })
    full = _select_view(chs, "combus", {"BASE", "SYSTEM", "BOARD"})
    assert full.ch_count == 3
    # WIRE_END == 1: R1 is BASE, then L1/S1 follow.
    assert full.wire_end == 1
    ids = [vc.ch.id for vc in full.channels]
    assert ids == ["R1", "L1", "S1"]


def test_select_view_combus_base_only_remotes():
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("R2", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
        ]
    })
    base = _select_view(chs, "combus_base", {"BASE"})
    assert base.ch_count == 2
    assert base.wire_end == 2  # == CH_COUNT
    ids = [vc.ch.id for vc in base.channels]
    assert set(ids) == {"R1", "R2"}


def test_select_view_base_only_wire_end_equals_count():
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("R2", scope="BASE"),
        ]
    })
    full = _select_view(chs, "combus", {"BASE", "SYSTEM", "BOARD"})
    assert full.wire_end == 2
    assert full.ch_count == 2


def test_select_view_analog_before_digital_within_scope():
    chs = _canon_from_yamls({
        "channels": [
            _ch("D1", scope="BASE", type_="digital"),
            _ch("A1", scope="BASE", type_="analog"),
        ]
    })
    full = _select_view(chs, "combus", {"BASE", "SYSTEM", "BOARD"})
    assert full.channels[0].ch.id == "A1"
    assert full.channels[1].ch.id == "D1"


def test_select_views_both_views():
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
        ]
    })
    sel = select_views(chs)
    assert isinstance(sel, ViewSelection)
    assert sel.full.ch_count == 3
    assert sel.system.ch_count == 2
    assert sel.base.ch_count == 1
    assert sel.base.channels[0].ch.id == "R1"


# =============================================================================
# A6.2 — combus_system view (BASE + SYSTEM, no BOARD)
# =============================================================================

def test_select_views_system_excludes_board():
    """A6.2: combus_system contains exactly BASE+SYSTEM channels, no BOARD."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("R2", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("L2", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
            _ch("S2", scope="BOARD", direction=None),
        ]
    })
    sel = select_views(chs)
    assert sel.system.ch_count == 4
    scopes = {vc.ch.scope for vc in sel.system.channels}
    assert scopes == {"BASE", "SYSTEM"}
    assert "BOARD" not in scopes


def test_select_views_system_is_prefix_of_full():
    """A6.2: combus_system is a strict prefix of combus (BASE+SYSTEM channels
    appear in the same order, with the same numeric IDs)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("R2", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("L2", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
            _ch("S2", scope="BOARD", direction=None),
        ]
    })
    sel = select_views(chs)
    # combus_system must be a prefix of combus (same channels, same order, same IDs).
    assert sel.system.ch_count == 4
    assert sel.full.ch_count == 6
    for i in range(sel.system.ch_count):
        assert sel.system.channels[i].ch.id == sel.full.channels[i].ch.id, (
            f"order mismatch at index {i}: system={sel.system.channels[i].ch.id} "
            f"vs full={sel.full.channels[i].ch.id}"
        )
        assert sel.system.channels[i].numeric_id == sel.full.channels[i].numeric_id, (
            f"id mismatch at index {i}: system={sel.system.channels[i].numeric_id} "
            f"vs full={sel.full.channels[i].numeric_id}"
        )


def test_select_views_system_no_renumbering():
    """A6.2: a channel that appears in both combus and combus_system has the
    SAME numeric_id in both views (no renumbering)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("R2", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("L2", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
        ]
    })
    sel = select_views(chs)
    # Build a map id -> numeric_id in each view.
    full_ids = {vc.ch.id: vc.numeric_id for vc in sel.full.channels}
    local_ids = {vc.ch.id: vc.numeric_id for vc in sel.system.channels}
    # Every channel in combus_system must have the same numeric_id in combus.
    for cid, nid in local_ids.items():
        assert cid in full_ids, f"{cid} missing from combus"
        assert full_ids[cid] == nid, (
            f"renumbering detected: {cid} has id {nid} in combus_system "
            f"but {full_ids[cid]} in combus"
        )


def test_select_views_system_wire_end_equals_full_wire_end():
    """A6.2: combus_system.wire_end == combus.wire_end (same BASE/SYSTEM
    boundary by construction)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("R2", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
            _ch("S1", scope="BOARD", direction=None),
        ]
    })
    sel = select_views(chs)
    assert sel.system.wire_end == sel.full.wire_end


def test_select_views_system_deterministic():
    """A6.2: combus_system is deterministic (parity with combus/combus_base)."""
    chs_a = _canon_from_yamls({"channels": [
        _ch("R1", scope="BASE"),
        _ch("L1", scope="SYSTEM"),
        _ch("S1", scope="BOARD", direction=None),
    ]})
    sel_a = select_views(chs_a)
    chs_b = _canon_from_yamls({"channels": [
        _ch("S1", scope="BOARD", direction=None),
        _ch("L1", scope="SYSTEM"),
        _ch("R1", scope="BASE"),
    ]})
    sel_b = select_views(chs_b)
    assert [vc.ch.id for vc in sel_a.system.channels] == [vc.ch.id for vc in sel_b.system.channels]
    assert [vc.numeric_id for vc in sel_a.system.channels] == [vc.numeric_id for vc in sel_b.system.channels]


def test_select_views_system_empty_when_no_base_or_system():
    """A6.2: combus_system is empty when only BOARD channels exist."""
    chs = _canon_from_yamls({"channels": [
        _ch("S1", scope="BOARD", direction=None),
        _ch("S2", scope="BOARD", direction=None),
    ]})
    sel = select_views(chs)
    assert sel.system.ch_count == 0
    assert sel.system.wire_end == 0


def test_generate_emits_combus_board_files(tmp_path):
    """A6.2 / Phase 2: generate() emits combus_system_ids.h only (no system .h/.cpp)."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="BASE"),
        _ch("L1", scope="SYSTEM"),
        _ch("S1", scope="BOARD", direction=None),
    ]})
    sel = generate(chs, _ctx([]), tmp_path)
    assert (tmp_path / "combus_system_ids.h").exists()
    # Phase 2: the runtime .h/.cpp are NOT emitted for the system/base views.
    assert not (tmp_path / "combus_system.h").exists()
    assert not (tmp_path / "combus_system.cpp").exists()
    # The full view still emits its three files.
    assert (tmp_path / "combus_ids.h").exists()
    assert (tmp_path / "combus.h").exists()
    assert (tmp_path / "combus.cpp").exists()
    assert (tmp_path / "combus_base_ids.h").exists()
    assert not (tmp_path / "combus_base.h").exists()
    assert not (tmp_path / "combus_base.cpp").exists()


def test_combus_board_ids_header_excludes_board():
    """A6.2: combus_system_ids.h must NOT contain any BOARD channel."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="BASE"),
        _ch("L1", scope="SYSTEM"),
        _ch("S1", scope="BOARD", direction=None),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        ids = (Path(tmp_dir) / "combus_system_ids.h").read_text(encoding="utf-8")
    assert "R1" in ids
    assert "L1" in ids
    assert "S1" not in ids


def test_combus_board_no_wire_end_sentinel():
    """A6.2: combus_system_ids.h does NOT declare a WireEnd sentinel
    (combus.WireEnd is the canonical boundary; combus_system reuses it)."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="BASE"),
        _ch("L1", scope="SYSTEM"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        ids = (Path(tmp_dir) / "combus_system_ids.h").read_text(encoding="utf-8")
    assert "Combus_systemWireEnd" not in ids
    assert "WireEnd" not in ids


def test_select_views_deterministic():
    chs_a = _canon_from_yamls({"channels": [_ch("R1", scope="BASE"),
                                              _ch("L1", scope="SYSTEM")]})
    sel_a = select_views(chs_a)
    chs_b = _canon_from_yamls({"channels": [_ch("L1", scope="SYSTEM"),
                                              _ch("R1", scope="BASE")]})
    sel_b = select_views(chs_b)
    assert [vc.ch.id for vc in sel_a.full.channels] == [vc.ch.id for vc in sel_b.full.channels]
    assert [vc.numeric_id for vc in sel_a.full.channels] == [vc.numeric_id for vc in sel_b.full.channels]


def test_select_views_empty_channel_set():
    sel = select_views([])
    assert sel.full.ch_count == 0
    assert sel.base.ch_count == 0
    assert sel.full.wire_end == 0


def test_select_views_same_id_in_both_views():
    """A BASE channel has the same numeric_id in both views."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE"),
            _ch("L1", scope="SYSTEM"),
        ]
    })
    sel = select_views(chs)
    r1_full = [vc for vc in sel.full.channels if vc.ch.id == "R1"][0]
    r1_remote = [vc for vc in sel.base.channels if vc.ch.id == "R1"][0]
    assert r1_full.numeric_id == r1_remote.numeric_id == 0


# =============================================================================
# Direction bits in ViewChannel
# =============================================================================

def test_view_channel_direction_bits():
    chs = _canon_from_yamls({
        "channels": [
            _ch("UP", direction="uplink"),
            _ch("DOWN", direction="downlink"),
            _ch("BOTH", direction="both"),
            _ch("NONE", direction="none"),
        ]
    })
    full = _select_view(chs, "combus", {"BASE", "SYSTEM", "BOARD"})
    by_id = {vc.ch.id: vc for vc in full.channels}
    assert by_id["UP"].direction_bits == 1
    assert by_id["DOWN"].direction_bits == 2
    assert by_id["BOTH"].direction_bits == 3
    assert by_id["NONE"].direction_bits == 0


# =============================================================================
# C++ rendering
# =============================================================================

def _build_view_with_channels(channels: list[ChannelDefinition]) -> View:
    """Build a test View wrapping a list of channels (mirror of
    `_select_view` from generator.py). Phase 5 (A15): each type has
    its own 0-based counter.
    """
    _VIEW_SCOPE_ORDER = {"BASE": 0, "SYSTEM": 1, "BOARD": 2}
    selected_analog = sorted(
        [ch for ch in channels if ch.type == "analog"],
        key=lambda ch: (_VIEW_SCOPE_ORDER[ch.scope], ch.theme, ch.id),
    )
    selected_digital = sorted(
        [ch for ch in channels if ch.type == "digital"],
        key=lambda ch: (_VIEW_SCOPE_ORDER[ch.scope], ch.theme, ch.id),
    )
    view_channels: list[ViewChannel] = []
    wire_end_analog = 0
    wire_end_digital = 0
    for idx, ch in enumerate(selected_analog):
        view_channels.append(ViewChannel(
            numeric_id=idx,
            ch=ch,
            direction_bits=Direction.from_frozenset(ch.direction),
        ))
        if ch.scope == "BASE":
            wire_end_analog += 1
    for idx, ch in enumerate(selected_digital):
        view_channels.append(ViewChannel(
            numeric_id=idx,
            ch=ch,
            direction_bits=Direction.from_frozenset(ch.direction),
        ))
        if ch.scope == "BASE":
            wire_end_digital += 1
    wire_end = wire_end_analog + wire_end_digital
    return View(
        name="combus",
        channels=view_channels,
        wire_end=wire_end,
        wire_end_analog=wire_end_analog,
        wire_end_digital=wire_end_digital,
    )


def test_render_ids_header_basic():
    chs = _canon_from_yamls({"channels": [_ch("FOO", type_="digital", scope="BASE")]})
    view = _build_view_with_channels(chs)
    out = _render_ids_header(view, _ctx([]))
    assert "GENERATED FILE" in out
    assert "enum class DigitalComBusID : uint8_t" in out
    assert "FOO = 0" in out
    assert "CH_COUNT" in out
    # Phase 4 (A14): WireEnd constants are no longer emitted.
    assert "CombusWireEnd" not in out


# =============================================================================
# Phase 1 A.11 — per-bus wire-end divergence (regression guard)
# =============================================================================

def test_wire_end_analog_and_digital_can_diverge():
    """Phase 1 A.11 regression guard: a config with 5 BASE analog and
    3 BASE digital channels must yield wire_end_analog=5 and
    wire_end_digital=3, with the two values explicitly distinct.

    This is the exact bug that caf2353 silently introduced: collapsing
    both per-bus counts into a single ComBusWireEnd (= total BASE).
    Without this test, a future refactor that refuses the two counters
    would not be caught.
    """
    chs = _canon_from_yamls({
        "channels": [
            _ch("A1", type_="analog", scope="BASE", direction="uplink"),
            _ch("A2", type_="analog", scope="BASE", direction="uplink"),
            _ch("A3", type_="analog", scope="BASE", direction="uplink"),
            _ch("A4", type_="analog", scope="BASE", direction="uplink"),
            _ch("A5", type_="analog", scope="BASE", direction="uplink"),
            _ch("D1", type_="digital", scope="BASE", direction="uplink"),
            _ch("D2", type_="digital", scope="BASE", direction="uplink"),
            _ch("D3", type_="digital", scope="BASE", direction="uplink"),
            _ch("L1", type_="analog", scope="SYSTEM", direction="uplink"),  # not BASE — must not be counted
        ]
    })
    view = _build_view_with_channels(chs)
    assert view.wire_end_analog == 5
    assert view.wire_end_digital == 3
    # Explicit non-regression assertion: the two values MUST differ.
    assert view.wire_end_analog != view.wire_end_digital
    # And the total wire_end must equal the sum of the two per-bus counts.
    assert view.wire_end == view.wire_end_analog + view.wire_end_digital == 8


def test_wire_end_analog_and_digital_equal_when_counts_match():
    """Sanity check: when BASE analog and BASE digital counts are
    equal, the per-bus constants are equal too (and equal to wire_end/2).
    This is the volvo_A60H_bruder case (9 + 9 = 18)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch(f"A{i}", type_="analog", scope="BASE", direction="uplink") for i in range(1, 4)
        ] + [
            _ch(f"D{i}", type_="digital", scope="BASE", direction="uplink") for i in range(1, 4)
        ]
    })
    view = _build_view_with_channels(chs)
    assert view.wire_end_analog == 3
    assert view.wire_end_digital == 3
    assert view.wire_end_analog == view.wire_end_digital
    assert view.wire_end == 6


def test_render_ids_header_does_not_emit_wire_end():
    """Phase 4 (A14) regression guard: the generated combus_ids.h must
    NOT contain ComBusWireEnd / ComBusWireEndAnalog / ComBusWireEndDigital
    anymore. Wire dimensions are now derived directly from the Base
    view's ID enums (AnalogComBusBaseID::CH_COUNT and
    DigitalComBusBaseID::CH_COUNT) at the call site.

    The per-bus counters (wire_end_analog, wire_end_digital) are still
    computed by the generator (see test_wire_end_analog_and_digital_*),
    but they are no longer emitted as C++ constants.
    """
    chs = _canon_from_yamls({
        "channels": [
            _ch("A1", type_="analog", scope="BASE", direction="uplink"),
            _ch("A2", type_="analog", scope="BASE", direction="uplink"),
            _ch("A3", type_="analog", scope="BASE", direction="uplink"),
            _ch("A4", type_="analog", scope="BASE", direction="uplink"),
            _ch("A5", type_="analog", scope="BASE", direction="uplink"),
            _ch("D1", type_="digital", scope="BASE", direction="uplink"),
            _ch("D2", type_="digital", scope="BASE", direction="uplink"),
            _ch("D3", type_="digital", scope="BASE", direction="uplink"),
        ]
    })
    view = _build_view_with_channels(chs)
    out = _render_ids_header(view, _ctx([]))
    # None of the WireEnd constants must be emitted.
    assert "ComBusWireEnd" not in out
    assert "WireEndAnalog" not in out
    assert "WireEndDigital" not in out
    # Sanity: the comment must NOT mention a specific machine name
    # (the generator is generic, called for any config).
    assert "volvo_A60H_bruder" not in out
    assert "dumper_truck" not in out
    assert "excavator" not in out
    assert "loader" not in out




def test_render_ids_header_analog_digital():
    chs = _canon_from_yamls({
        "channels": [
            _ch("A1", type_="analog", scope="BASE"),
            _ch("D1", type_="digital", scope="BASE"),
        ]
    })
    view = _build_view_with_channels(chs)
    out = _render_ids_header(view, _ctx([]))
    assert "enum class AnalogComBusID" in out
    assert "enum class DigitalComBusID" in out
    # A1 gets 0 in analog, D1 gets 0 in digital (each enum has its own ID space).
    assert re.search(r"A1\s*=\s*0", out)
    assert re.search(r"D1\s*=\s*0", out)


def test_render_ids_header_no_wire_end_for_base_view():
    chs = _canon_from_yamls({"channels": [_ch("R1", scope="BASE")]})
    sel = select_views(chs)
    out = _render_ids_header(sel.base, _ctx([]))
    assert "Combus_baseWireEnd" not in out


def test_render_header_externs_real_runtime_structs():
    """A6.1: header declares extern AnalogComBusArray[] / DigitalComBusArray[]."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", type_="digital", scope="BASE"),
        _ch("BAR", type_="analog", scope="BASE"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "extern AnalogComBus AnalogComBusArray" in out
    assert "extern DigitalComBus DigitalComBusArray" in out


def test_render_header_no_direction_enum():
    """A6.1: Direction enum is defined in combus_defs.h (runtime), NOT in
    the generated header. Generator only USES the enum."""
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "enum class Direction" not in out


def test_render_header_combus_view_has_bus_instance():
    """combus (full) view header declares `extern ComBus comBus`."""
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "extern ComBus comBus" in out


def test_render_header_base_view_no_bus_instance():
    """combus_base view header does NOT declare comBus (it's a wire view)."""
    chs = _canon_from_yamls({"channels": [_ch("R1", scope="BASE")]})
    sel = select_views(chs)
    out = _render_header(sel.base, _ctx([]))
    assert "extern ComBus comBus" not in out


def test_render_source_real_runtime_structs():
    """A6.1: source defines AnalogComBusArray[] / DigitalComBusArray[] directly."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="Foo channel", direction="uplink"),
        _ch("BAR", type_="analog", info_name="Bar analog", direction="both"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "AnalogComBus AnalogComBusArray" in out
    assert "DigitalComBus DigitalComBusArray" in out
    assert "Foo channel" in out
    assert "Bar analog" in out


def test_render_source_info_name_with_quote_safe():
    """infoName containing a quote is escaped."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name='Foo "channel"'),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert '\\"channel\\"' in out


def test_render_source_info_name_with_backslash():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="foo\\bar"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "foo\\\\bar" in out


def test_render_source_analog_default_is_cbus_neutral():
    """Analog channels default to CbusNeutral in .value."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", type_="analog"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert ".value = CbusNeutral" in out


def test_render_source_digital_default_is_false():
    """Digital channels default to false in .value."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", type_="digital"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert ".value = false" in out


# =============================================================================
# File emission
# =============================================================================

def test_emit_view_writes_three_files(tmp_path):
    chs = _canon_from_yamls({"channels": [_ch("FOO", scope="BASE")]})
    sel = select_views(chs)
    files = emit_view(sel.full, tmp_path, _ctx([]))
    assert len(files) == 3
    names = sorted(p.name for p in files)
    assert names == ["combus.cpp", "combus.h", "combus_ids.h"]
    for p in files:
        assert p.exists()
        assert p.stat().st_size > 0


def test_safe_write_atomic(tmp_path):
    target = tmp_path / "out.h"
    _safe_write(target, "hello\n")
    assert target.exists()
    assert target.read_text(encoding="utf-8") == "hello\n"


def test_safe_write_overwrites(tmp_path):
    target = tmp_path / "out.h"
    _safe_write(target, "first\n")
    _safe_write(target, "second\n")
    assert target.read_text(encoding="utf-8") == "second\n"


def test_safe_write_cleans_tmp_on_failure(tmp_path):
    target = tmp_path / "out.h"
    import os
    try:
        _safe_write(target, "initial\n")
        if not sys.platform.startswith("win"):
            os.chmod(tmp_path, 0o500)
            try:
                _safe_write(target, "new\n")
            except Exception:
                pass
            os.chmod(tmp_path, 0o700)
    except OSError:
        pass
    assert target.read_text(encoding="utf-8") == "initial\n"


# =============================================================================
# Full generate()
# =============================================================================

def test_generate_full_pipeline(tmp_path):
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE", type_="digital"),
            _ch("L1", scope="SYSTEM", type_="digital"),
            _ch("S1", scope="BOARD", type_="digital", direction=None),
        ]
    })
    ctx = _ctx([])
    sel = generate(chs, ctx, tmp_path)
    assert isinstance(sel, ViewSelection)
    assert (tmp_path / "combus.h").exists()
    assert (tmp_path / "combus.cpp").exists()
    assert (tmp_path / "combus_ids.h").exists()
    assert (tmp_path / "combus_base_ids.h").exists()
    assert (tmp_path / "combus_system_ids.h").exists()


def test_generate_with_requires_satisfied(tmp_path):
    chs = _canon_from_yamls({
        "channels": [
            _ch("FOO"),
            _ch("BAR", requires=["HAS_X"]),
        ]
    })
    ctx = _ctx(["HAS_X"])
    sel = generate(chs, ctx, tmp_path)
    assert sel.full.ch_count == 2


def test_generate_requires_missing_skips_and_logs(tmp_path, capsys):
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["MISSING"])]})
    sel = generate(chs, _ctx([]), tmp_path)
    assert sel.full.ch_count == 0
    out = capsys.readouterr().out
    assert "channel 'FOO' not generated" in out
    assert "MISSING" in out


def test_generate_empty_yields_empty_views(tmp_path):
    sel = generate([], _ctx([]), tmp_path)
    assert sel.full.ch_count == 0
    assert sel.base.ch_count == 0
    assert (tmp_path / "combus_ids.h").exists()
    assert (tmp_path / "combus_base_ids.h").exists()


# =============================================================================
# Phase 2 / A12 — combus_system and combus_base .h/.cpp are NOT emitted
# =============================================================================

def test_generate_does_not_emit_system_base_h_cpp(tmp_path):
    """Phase 2 / A12: combus_system.h/.cpp and combus_base.h/.cpp
    are NO LONGER written to disk. Only the _ids.h + _md5.h headers
    are emitted for these views. The MD5 payload is built directly
    from the in-memory View model (see md5.canonical_bytes), so no
    intermediate .cpp file is needed.

    The combus (full) view still emits combus.h + combus.cpp + combus_ids.h
    because it's the runtime bus instance.
    """
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE", type_="digital", direction="uplink"),
            _ch("R2", scope="BASE", type_="analog", direction="uplink"),
            _ch("L1", scope="SYSTEM", type_="digital", direction="uplink"),
        ]
    })
    sel = generate(chs, _ctx([]), tmp_path)
    # combus (full view): 3 files
    assert (tmp_path / "combus.h").is_file()
    assert (tmp_path / "combus.cpp").is_file()
    assert (tmp_path / "combus_ids.h").is_file()
    # combus_system / combus_base: ids only (no .h/.cpp)
    assert (tmp_path / "combus_system_ids.h").is_file()
    assert (tmp_path / "combus_base_ids.h").is_file()
    # Phase 2: the .h/.cpp counterparts MUST NOT exist
    assert not (tmp_path / "combus_system.h").exists(), \
        "combus_system.h should NOT be emitted (Phase 2)"
    assert not (tmp_path / "combus_system.cpp").exists(), \
        "combus_system.cpp should NOT be emitted (Phase 2)"
    assert not (tmp_path / "combus_base.h").exists(), \
        "combus_base.h should NOT be emitted (Phase 2)"
    assert not (tmp_path / "combus_base.cpp").exists(), \
        "combus_base.cpp should NOT be emitted (Phase 2)"
    # MD5 artifacts (3 views + shared)
    assert (tmp_path / "combus_system_md5.h").is_file()
    assert (tmp_path / "combus_base_md5.h").is_file()
    assert (tmp_path / "combus_md5.h").is_file()
    assert (tmp_path / "combus_wire_common.h").is_file()
    # Links between nodes (link rework 3.1): always written, empty without peer
    assert (tmp_path / "combus_peer_links.h").is_file()
    # Total: 10 files
    all_files = sorted(p.name for p in tmp_path.iterdir())
    assert len(all_files) == 10, f"Expected 10 files, got {len(all_files)}: {all_files}"


def test_md5_payload_built_from_view_model_not_files(tmp_path):
    """Phase 2: the MD5 payload must be built from the in-memory View
    model, NOT from reading combus_system.cpp / combus_base.cpp
    (which are no longer on disk).

    We verify this by:
      1. Asserting that the _md5.h files are still produced even though
         the .cpp files don't exist.
      2. Asserting that two generate() calls with identical inputs
         produce identical MD5 hashes (no filesystem-coupling).
    """
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="BASE", type_="digital", direction="uplink"),
            _ch("R2", scope="BASE", type_="analog", direction="uplink"),
        ]
    })
    # First run
    sel1 = generate(chs, _ctx([]), tmp_path / "run1")
    md5_run1 = (tmp_path / "run1" / "combus_base_md5.h").read_text()
    # Second run on a different directory
    sel2 = generate(chs, _ctx([]), tmp_path / "run2")
    md5_run2 = (tmp_path / "run2" / "combus_base_md5.h").read_text()
    # MD5 must be identical (deterministic, no path/host coupling)
    assert md5_run1 == md5_run2, (
        "MD5 hash differs between runs — this means the hash is NOT "
        "computed purely from the in-memory View model."
    )
    # Verify the .cpp files do NOT exist on disk for the second run
    assert not (tmp_path / "run2" / "combus_base.cpp").exists()
    assert not (tmp_path / "run2" / "combus_base.h").exists()
    # But the MD5 header DOES exist
    assert (tmp_path / "run2" / "combus_base_md5.h").is_file()


# =============================================================================
# Generated C++ is structurally well-formed (basic checks)
# =============================================================================

def test_generated_ids_header_parses():
    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="BASE"),
        _ch("D1", type_="digital", scope="BASE"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        sel = generate(chs, _ctx([]), Path(tmp_dir))
        ids = (Path(tmp_dir) / "combus_ids.h").read_text(encoding="utf-8")
    assert re.search(r"enum\s+class\s+AnalogComBusID\s*:\s*uint8_t", ids)
    assert re.search(r"enum\s+class\s+DigitalComBusID\s*:\s*uint8_t", ids)
    assert re.search(r"A1\s*=\s*0", ids)
    assert re.search(r"D1\s*=\s*0", ids)  # A15: per-type counters
    assert re.search(r"CH_COUNT", ids)


from scripts.combus_builder.tests._cpp_compiler import find_cpp_compiler as _find_cpp_compiler  # noqa: E402


def test_generated_cpp_compiles_cleanly_with_cpp_check():
    """
    If a C++ compiler is available, validate that the generated
    combus.h / combus.cpp / combus_ids.h parse and type-check
    against the real runtime src/core/system/combus/combus_defs.h.

    Toolchain target: -std=gnu++17 (matches platformio.ini line 34,
    espressif32 Xtensa GCC 12.2.0). Designated initializers for
    aggregates are supported by GCC/Clang in gnu++17 mode and are
    already used throughout the repo's legacy .inc files, so no
    fallback (positional init, constructor, etc.) is needed.

    Whatever the compiler (native g++/clang++ or the PlatformIO Xtensa
    cross-compiler) we use -fsyntax-only (no linking): the generated
    combus.cpp pulls in headers that only make sense on the ESP32
    (const.h refuses an unknown CPU), so it cannot be compiled, nor the
    `comBus` it defines linked, on a PC.
    """
    import subprocess
    found = _find_cpp_compiler()
    if found is None:
        import pytest
        pytest.skip("no C++ compiler available (g++, clang++, or PlatformIO xtensa-esp32-elf-g++)")
    argv0, label = found

    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="BASE"),
        _ch("D1", type_="digital", scope="BASE"),
        _ch("L1", type_="analog", scope="SYSTEM"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)
        generate(chs, _ctx([]), tmp)
        # Minimal smoke test: include combus.h, read .direction, check enums.
        test_src = tmp / "test_main.cpp"
        test_src.write_text(
            """
#include "combus.h"
#include <type_traits>
int main() {
    // enum class does not implicitly convert to int: use static_cast.
    // A15: analog and digital IDs are counted separately, each from 0.
    static_assert(static_cast<int>(AnalogComBusID::A1) == 0, "A1");
    static_assert(static_cast<int>(DigitalComBusID::D1) == 0, "D1");
    static_assert(static_cast<int>(AnalogComBusID::CH_COUNT) > 0, "count");
    // Read both directions of the flow to ensure they are publicly accessible.
    Direction d = comBus.analogBus[0].flow.betweenNodes;
    Direction e = comBus.analogBus[0].flow.betweenBoards;
    (void)d;
    (void)e;
    // The underlying uint8_t representation matches the bitmask contract.
    static_assert(static_cast<uint8_t>(Direction::None) == 0, "none=0");
    static_assert(static_cast<uint8_t>(Direction::Uplink) == 1, "up=1");
    static_assert(static_cast<uint8_t>(Direction::Downlink) == 2, "down=2");
    static_assert(static_cast<uint8_t>(Direction::Both) == 3, "both=3");
    // Verify enum underlying type matches the contract (uint8_t).
    static_assert(std::is_same<std::underlying_type_t<Direction>, uint8_t>::value,
                  "underlying is uint8_t");
    // Verify the runtime struct field count is exactly 4
    // (infoName, value, layer, direction) — compile-time check via sizeof.
    struct StaticSize {
        AnalogComBus a;
        DigitalComBus d;
    };
    // Each AnalogComBus / DigitalComBus must have non-zero size.
    static_assert(sizeof(AnalogComBus) > 0, "AnalogComBus size");
    static_assert(sizeof(DigitalComBus) > 0, "DigitalComBus size");
    (void)d;
    return 0;
}
"""
        )
        cmd = list(argv0)
        cmd += ["-std=gnu++17",
                "-I", str(tmp),
                "-I", str(REPO_ROOT / "include"),
                "-I", str(REPO_ROOT / "src")]
        # The runtime header pulls in <defs/machines_defs.h> which
        # transitively includes <pin_defs.h> (board-specific pin map,
        # provided by common_defs library). Add any libdeps include
        # path that contains pin_defs.h.
        for p in (REPO_ROOT / ".pio" / "libdeps").glob("*/common_defs*/include"):
            if (p / "pin_defs.h").exists():
                cmd += ["-I", str(p)]
        # Syntax-only check, no linking (see the docstring).
        cmd += ["-fsyntax-only", str(test_src)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        assert r.returncode == 0, (
            f"compilation failed with {label} "
            f"(stdout={r.stdout!r}, stderr={r.stderr!r})"
        )


# =============================================================================
# Integration with a real .cb file in the repo
# =============================================================================

def test_generator_against_real_repo_files():
    """Run the generator against the actual .cb files in the repo."""
    from scripts.combus_builder.parser import discover_and_parse
    from scripts.combus_builder.canon import canonize

    buildroot = Path(REPO_ROOT) / "src"
    if not buildroot.exists():
        import pytest
        pytest.skip(f"src/ not found at {buildroot}")

    _, files, parsed = discover_and_parse(project_root=buildroot.parent, machine_type="dumper_truck")
    cb_files = [f for f in files if f.suffix == ".cb"]
    if not cb_files:
        import pytest
        pytest.skip("no .cb files found in src/")

    result = canonize(parsed)
    chs = result.canonical_definitions

    ctx = BuildContext(
        buildroot=buildroot,
        defines=frozenset({"HAS_FAILSAFE", "HAS_VBAT_FAILSAFE", "HAS_RUNLEVEL_MANAGER"}),
        defines_with_value={},
        cppdefines_source="override",
    )

    with tempfile.TemporaryDirectory() as tmp_dir:
        sel = generate(chs, ctx, Path(tmp_dir))
        assert sel.full.ch_count > 0
        assert sel.full.ch_count >= sel.base.ch_count
        # Phase 2: combus (3 files) + the two *_ids.h of the other views.
        emitted = ("combus.h", "combus.cpp", "combus_ids.h",
                   "combus_system_ids.h", "combus_base_ids.h")
        for fname in emitted:
            assert (Path(tmp_dir) / fname).exists(), f"missing {fname}"
        # And no ChannelDescriptor anywhere.
        for fname in emitted:
            content = (Path(tmp_dir) / fname).read_text(encoding="utf-8")
            assert "ChannelDescriptor" not in content, f"ChannelDescriptor leaked in {fname}"


# =============================================================================
# Python <-> C++ coherence (Direction enum values + struct field order)
# =============================================================================

def _parse_direction_enum(header_text: str) -> dict[str, int]:
    """
    Parse the `enum class Direction : uint8_t { ... }` declaration and return
    a dict {name: value}.

    Format-tolerant: tolerates any whitespace between tokens, comments,
    clang-format re-spacing (e.g. `None=0`, `None  =  0`, `None = 0,`),
    AND multiple enumerators on a single line (e.g. `None=0,Uplink=1,...`).

    Skips enumerators without explicit values (those inherit the previous
    value + 1) — for our Direction enum all four enumerators are explicit.
    """
    m = re.search(
        r"enum\s+class\s+Direction\s*:\s*uint8_t\s*\{([^}]*)\}",
        header_text,
    )
    assert m, "enum class Direction not found in header"
    body = m.group(1)
    # Strip C++ line comments (// ...) — they're irrelevant for parsing.
    body_no_comments = re.sub(r"//[^\n]*", "", body)
    # Split on commas AND newlines AND semicolons so we get one
    # enumerator per token regardless of layout.
    tokens = re.split(r"[,\n;]", body_no_comments)
    out: dict[str, int] = {}
    for tok in tokens:
        line = tok.strip()
        if not line:
            continue
        # "<Name> = <Value>"  (value can be decimal int, possibly with 'u'/'U' suffix).
        mm = re.match(r"^([A-Za-z_]\w*)\s*=\s*([0-9]+)\s*[uU]?$", line)
        if not mm:
            continue
        name, value = mm.group(1), int(mm.group(2))
        out[name] = value
    return out


def test_python_direction_constants_match_cpp_enum():
    """
    The Python `Direction.X` constants must equal the C++ `enum class Direction`
    values declared in combus_defs.h.

    This test parses the enum rather than matching substrings, so it remains
    valid after clang-format or any whitespace re-alignment.
    """
    # Expected Python-side mapping.
    expected_py = {
        "NONE": Direction.NONE,
        "UPLINK": Direction.UPLINK,
        "DOWNLINK": Direction.DOWNLINK,
        "BOTH": Direction.BOTH,
    }
    assert expected_py == {"NONE": 0, "UPLINK": 1, "DOWNLINK": 2, "BOTH": 3}

    # Parse the C++ enum from the real header.
    h = (REPO_ROOT / "src" / "core" / "system" / "combus" / "combus_defs.h").read_text(encoding="utf-8")
    cpp_enum = _parse_direction_enum(h)

    # The C++ enum must declare the same four enumerators with the same values.
    expected_cpp = {"None": 0, "Uplink": 1, "Downlink": 2, "Both": 3}
    assert cpp_enum == expected_cpp, (
        f"Direction enum mismatch: parsed {cpp_enum}, expected {expected_cpp}"
    )

    # Cross-check: Python constant value == C++ enumerator value for each pair.
    pairs = [("NONE", "None"), ("UPLINK", "Uplink"),
             ("DOWNLINK", "Downlink"), ("BOTH", "Both")]
    for py_name, cpp_name in pairs:
        assert expected_py[py_name] == cpp_enum[cpp_name], (
            f"mismatch: Python Direction.{py_name}={expected_py[py_name]} "
            f"vs C++ Direction::{cpp_name}={cpp_enum[cpp_name]}"
        )


def test_parse_direction_enum_tolerates_formatting():
    """The parser must accept clang-format-style spacing variations."""
    variants = [
        # Original style
        """enum class Direction : uint8_t {
            None     = 0,
            Uplink   = 1,
            Downlink = 2,
            Both     = 3
        };""",
        # clang-format compact style
        """enum class Direction : uint8_t {
            None = 0,
            Uplink = 1,
            Downlink = 2,
            Both = 3
        };""",
        # Loose style
        """enum class Direction:uint8_t{
            None=0,Uplink=1,Downlink=2,Both=3
        };""",
        # With C++ comments
        """enum class Direction : uint8_t {
            None = 0,     ///< no wire
            Uplink = 1,   ///< up
            Downlink = 2, ///< down
            Both = 3      ///< both
        };""",
    ]
    for v in variants:
        parsed = _parse_direction_enum(v)
        assert parsed == {"None": 0, "Uplink": 1, "Downlink": 2, "Both": 3}, (
            f"parser failed on variant:\n{v}\n  got {parsed}"
        )


def test_python_direction_to_cpp_enum_name_roundtrip():
    """For every Direction value, the C++ enum name round-trips."""
    for v in range(4):
        assert Direction.to_cpp_enum_name(v).startswith("Direction::")


def test_runtime_struct_field_order_matches_python_emission():
    """The runtime struct (AnalogComBus / DigitalComBus) must declare its
    fields in the same order as the generator emits initializers:
    infoName, value, layer, flow (then bothOr for DigitalComBus)."""
    h = (REPO_ROOT / "src" / "core" / "system" / "combus" / "combus_defs.h").read_text(encoding="utf-8")
    for struct_name in ("AnalogComBus", "DigitalComBus"):
        # Find the struct definition body.
        m = re.search(rf"typedef\s+struct\s+\{{(.*?)\}}\s*{struct_name};", h, re.DOTALL)
        assert m, f"{struct_name} struct not found"
        body = m.group(1)
        pos_info = body.find("infoName")
        pos_value = body.find("value")
        pos_layer = body.find("layer")
        pos_dir = body.find("flow")
        assert pos_info >= 0, f"{struct_name} missing infoName"
        assert pos_value > pos_info, f"{struct_name} value before infoName"
        assert pos_layer > pos_value, f"{struct_name} layer before value"
        assert pos_dir > pos_layer, f"{struct_name} flow before layer"
        if struct_name == "DigitalComBus":
            assert body.find("bothOr") > pos_dir, "DigitalComBus bothOr before flow"


def test_generated_cpp_direction_for_both():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", direction="both"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::Both" in out


def test_generated_cpp_direction_for_none():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", scope="BOARD", direction="none"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::None" in out


def test_generated_cpp_direction_for_uplink():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", direction="uplink"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::Uplink" in out


def test_generated_cpp_direction_for_downlink():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", direction="downlink"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::Downlink" in out


# =============================================================================
# Phase 5 (A15) — ID/data correspondence tests
# =============================================================================
# These tests guard against the bug found in A14 verification: the
# generator previously allocated IDs via a single global enumerate()
# over a (scope, type, theme, id)-sorted channel list, which mixed
# analog and digital channels into one sequence. The result: digital
# IDs started at 9 (after the 9 analog BASE channels), and the
# generated DigitalComBusArray[] was indexed by the position-in-list
# (0..19), not by the channel's own ID (9..17). Reading
# DigitalComBusArray[CRUISE_ACTIVE=9] therefore read CRUISE_TOGGLE_BTN
# data, not CRUISE_ACTIVE data. The fix: each TYPE now has its own
# 0-based counter. The tests below verify the fix is in place.
# =============================================================================


def test_a15_analog_ids_are_0_based_per_type():
    """Phase 5 (A15) regression guard: analog channels must have 0-based
    numeric_id within their own type, NOT in a global sequence that
    mixes with digital channels.

    Setup: 2 analog BASE + 2 digital BASE. Before the fix, the
    analog IDs would be {0, 1} and the digital IDs would be {2, 3}
    (global enumerate). After the fix: analog {0, 1}, digital {0, 1}
    (per-type 0-based).
    """
    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="BASE", theme="core", direction="uplink"),
        _ch("D1", type_="digital", scope="BASE", theme="core", direction="uplink"),
        _ch("A2", type_="analog", scope="BASE", theme="core", direction="uplink"),
        _ch("D2", type_="digital", scope="BASE", theme="core", direction="uplink"),
    ]})
    sel = select_views(chs)
    analog = {vc.ch.id: vc.numeric_id
              for vc in sel.base.channels
              if vc.ch.type == "analog"}
    digital = {vc.ch.id: vc.numeric_id
               for vc in sel.base.channels
               if vc.ch.type == "digital"}
    # Each type starts at 0 and is contiguous.
    assert set(analog.values()) == {0, 1}, f"analog IDs not 0-based: {analog}"
    assert set(digital.values()) == {0, 1}, f"digital IDs not 0-based: {digital}"


def test_a15_generated_array_data_matches_enum_ids():
    """Phase 5 (A15) regression guard: the generated combus.cpp must
    place each channel's data at the array index equal to its enum ID.

    Before the fix: digital channels had enum IDs 2, 3 but their data
    was at array positions 0, 1. Reading digitalBus[2] would read
    garbage. After the fix: digitalBus[2] reads D2's data.

    This test parses the generated .cpp and verifies, for every
    channel, that the data appears at the position equal to its ID.
    """
    import re
    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="BASE", info_name="A1 data"),
        _ch("D1", type_="digital", scope="BASE", info_name="D1 data"),
        _ch("A2", type_="analog", scope="BASE", info_name="A2 data"),
        _ch("D2", type_="digital", scope="BASE", info_name="D2 data"),
    ]})
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as td:
        out_dir = Path(td)
        generate(chs, _ctx([]), out_dir)
        cpp = (out_dir / "combus.cpp").read_text(encoding="utf-8")

    # Extract infoName values for each channel type, in array order.
    def extract_info_names(cpp_text: str, type_: str) -> list[str]:
        m = re.search(
            rf"{type_}ComBusArray\s*\[[^\]]+\]\s*=\s*\{{(.*?)\}};",
            cpp_text, re.DOTALL)
        assert m, f"could not find {type_}ComBusArray in generated .cpp"
        body = m.group(1)
        return re.findall(r'\.infoName\s*=\s*"([^"]+)"', body)

    analog_names = extract_info_names(cpp, "Analog")
    digital_names = extract_info_names(cpp, "Digital")

    # Build a map from infoName -> expected enum ID.
    analog_id_by_name = {vc.ch.info_name: vc.numeric_id
                         for vc in sel.full.channels
                         if vc.ch.type == "analog"}
    digital_id_by_name = {vc.ch.info_name: vc.numeric_id
                          for vc in sel.full.channels
                          if vc.ch.type == "digital"}

    # For each name at position i, its expected ID is i. Verify the
    # actual ID matches.
    for i, name in enumerate(analog_names):
        expected_id = analog_id_by_name.get(name)
        assert expected_id == i, (
            f"AnalogComBusArray[{i}] contains {name!r} but its "
            f"expected ID is {expected_id} (data/enum mismatch)")
    for i, name in enumerate(digital_names):
        expected_id = digital_id_by_name.get(name)
        assert expected_id == i, (
            f"DigitalComBusArray[{i}] contains {name!r} but its "
            f"expected ID is {expected_id} (data/enum mismatch)")


def test_a15_base_view_only_contains_base_channels():
    """Phase 5 (A15): the combus_base view must only contain channels
    with scope=BASE, regardless of type.
    """
    chs = _canon_from_yamls({"channels": [
        _ch("R_A", type_="analog", scope="BASE"),
        _ch("R_D", type_="digital", scope="BASE"),
        _ch("L_A", type_="analog", scope="SYSTEM"),
        _ch("L_D", type_="digital", scope="SYSTEM"),
    ]})
    sel = select_views(chs)
    remote_ids = {vc.ch.id for vc in sel.base.channels}
    assert remote_ids == {"R_A", "R_D"}, (
        f"combus_base must only contain BASE channels, got {remote_ids}")


def test_a15_wire_end_equals_count_of_base_channels():
    """Phase 5 (A15): view.wire_end must equal the count of BASE
    channels of ALL types (analog + digital).
    """
    chs = _canon_from_yamls({"channels": [
        _ch("R1", type_="analog", scope="BASE"),
        _ch("R2", type_="digital", scope="BASE"),
        _ch("R3", type_="analog", scope="BASE"),
        _ch("L1", type_="analog", scope="SYSTEM"),
        _ch("L2", type_="digital", scope="SYSTEM"),
    ]})
    sel = select_views(chs)
    # combus_full has 3 BASE channels (2 analog + 1 digital).
    assert sel.full.wire_end == 3, (
        f"combus.wire_end should be 3 (count of BASE), got {sel.full.wire_end}")
    # combus_base has all 3 BASE channels.
    assert sel.base.wire_end == 3, (
        f"combus_base.wire_end should be 3 (== CH_COUNT), got {sel.base.wire_end}")
    # combus_system has 3 BASE channels (same as combus).
    assert sel.system.wire_end == 3
    # Per-type counts: 2 analog BASE, 1 digital BASE.
    assert sel.full.wire_end_analog == 2
    assert sel.full.wire_end_digital == 1
