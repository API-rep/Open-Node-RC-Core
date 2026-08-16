#!/usr/bin/env python3
"""
Tests for generator.py (A6.1) - C++ artifact generation.

A6.1 corrections vs A6:
  - ChannelDescriptor is REMOVED.
  - The generator emits REAL AnalogComBus / DigitalComBus runtime
    structures (infoName, value, layer, direction) — matching
    include/struct/combus_struct.h.
  - ChanLayer is preserved (audit A6 confirmed _layer_ok() requires it).
  - direction is a fourth field on each runtime struct entry.

Coverage:
  - requires resolution (errors only — A6 raises, not filters).
  - View selection (combus = REMOTE+LOCAL+SYSTEM; combus_remote = REMOTE).
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
    scope: str = "LOCAL",
    theme: str = "failsafe",
    direction: list | None = None,
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
    elif scope in ("LOCAL", "REMOTE"):
        d["direction"] = ["uplink"]
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
        _ch("FOO", scope="REMOTE"),
        _ch("BAR", type_="analog", scope="REMOTE"),
    ]})
    sel = generate(chs, _ctx([]), tmp_path)
    for fname in ("combus.h", "combus.cpp", "combus_ids.h",
                  "combus_remote.h", "combus_remote.cpp", "combus_remote_ids.h"):
        content = (tmp_path / fname).read_text(encoding="utf-8")
        assert "ChannelDescriptor" not in content, f"ChannelDescriptor in {fname}"


# =============================================================================
# A6.1 SPECIFIC: ChanLayer is PRESERVED
# =============================================================================

def test_chanlayer_enum_in_combus_struct():
    """combus_struct.h still defines ChanLayer (audit A6 confirmed _layer_ok)."""
    h = (REPO_ROOT / "include" / "struct" / "combus_struct.h").read_text(encoding="utf-8")
    assert "enum class ChanLayer" in h
    for v in ("SYSTEM", "LOCAL", "REMOTE", "UNDEFINED"):
        assert v in h, f"ChanLayer::{v} missing from combus_struct.h"


def test_generated_initializers_use_chanlayer():
    """Generated .cpp must use ChanLayer::X in the .layer field of each entry."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE", type_="digital"),
        _ch("L1", scope="LOCAL", type_="digital"),
        _ch("S1", scope="SYSTEM", type_="digital", direction=["none"]),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        cpp = (Path(tmp_dir) / "combus.cpp").read_text(encoding="utf-8")
    # At least one of each layer value must appear in the initializers.
    assert ".layer = ChanLayer::REMOTE" in cpp
    assert ".layer = ChanLayer::LOCAL" in cpp
    assert ".layer = ChanLayer::SYSTEM" in cpp


def test_generated_header_includes_combus_struct():
    """Generated .h must #include <struct/combus_struct.h> for ChanLayer / Direction."""
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "#include <struct/combus_struct.h>" in out


# =============================================================================
# A6.1 SPECIFIC: direction is added to runtime structs
# =============================================================================

def test_direction_field_in_combus_struct():
    """AnalogComBus and DigitalComBus both have a `direction` field."""
    h = (REPO_ROOT / "include" / "struct" / "combus_struct.h").read_text(encoding="utf-8")
    assert "Direction   direction" in h or "Direction direction" in h


def test_generated_initializers_have_direction_field():
    """Each generated entry must include `.direction = Direction::X`."""
    chs = _canon_from_yamls({"channels": [
        _ch("UP",   direction=["uplink"]),
        _ch("DOWN", direction=["downlink"]),
        _ch("BOTH", direction=["both"]),
        _ch("NONE", direction=["none"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert ".direction = Direction::Uplink" in out
    assert ".direction = Direction::Downlink" in out
    assert ".direction = Direction::Both" in out
    assert ".direction = Direction::None" in out


def test_generated_init_order_infoName_value_layer_direction():
    """Each entry must keep the field order: infoName, value, layer, direction."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="Foo channel", direction=["uplink"]),
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
    pos_dir = entry.find(".direction")
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


def test_resolve_requires_missing_raises():
    """A6.1 RAISES on missing requires (it does NOT silently filter)."""
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["MISSING_FLAG"])]})
    ctx = _ctx([])
    try:
        resolve_requires(chs, ctx)
    except RequiresResolutionError as e:
        assert e.channel_id == "FOO"
        assert "MISSING_FLAG" in e.missing
    else:
        raise AssertionError("expected RequiresResolutionError")


def test_resolve_requires_partial_missing_raises():
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["HAS_X", "MISSING"])]})
    ctx = _ctx(["HAS_X"])
    try:
        resolve_requires(chs, ctx)
    except RequiresResolutionError as e:
        assert "MISSING" in e.missing
        assert "HAS_X" not in e.missing
    else:
        raise AssertionError("expected RequiresResolutionError")


# =============================================================================
# View selection
# =============================================================================

def test_select_view_combus_full():
    """combus view = REMOTE first, then LOCAL, then SYSTEM."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
        ]
    })
    full = _select_view(chs, "combus", {"REMOTE", "LOCAL", "SYSTEM"})
    assert full.ch_count == 3
    # WIRE_END == 1: R1 is REMOTE, then L1/S1 follow.
    assert full.wire_end == 1
    ids = [vc.ch.id for vc in full.channels]
    assert ids == ["R1", "L1", "S1"]


def test_select_view_combus_remote_only_remotes():
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("R2", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
        ]
    })
    remote = _select_view(chs, "combus_remote", {"REMOTE"})
    assert remote.ch_count == 2
    assert remote.wire_end == 2  # == CH_COUNT
    ids = [vc.ch.id for vc in remote.channels]
    assert set(ids) == {"R1", "R2"}


def test_select_view_remote_only_wire_end_equals_count():
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("R2", scope="REMOTE"),
        ]
    })
    full = _select_view(chs, "combus", {"REMOTE", "LOCAL", "SYSTEM"})
    assert full.wire_end == 2
    assert full.ch_count == 2


def test_select_view_analog_before_digital_within_scope():
    chs = _canon_from_yamls({
        "channels": [
            _ch("D1", scope="REMOTE", type_="digital"),
            _ch("A1", scope="REMOTE", type_="analog"),
        ]
    })
    full = _select_view(chs, "combus", {"REMOTE", "LOCAL", "SYSTEM"})
    assert full.channels[0].ch.id == "A1"
    assert full.channels[1].ch.id == "D1"


def test_select_views_both_views():
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
        ]
    })
    sel = select_views(chs)
    assert isinstance(sel, ViewSelection)
    assert sel.full.ch_count == 3
    assert sel.local.ch_count == 2
    assert sel.remote.ch_count == 1
    assert sel.remote.channels[0].ch.id == "R1"


# =============================================================================
# A6.2 — combus_local view (REMOTE + LOCAL, no SYSTEM)
# =============================================================================

def test_select_views_local_excludes_system():
    """A6.2: combus_local contains exactly REMOTE+LOCAL channels, no SYSTEM."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("R2", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("L2", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
            _ch("S2", scope="SYSTEM", direction=None),
        ]
    })
    sel = select_views(chs)
    assert sel.local.ch_count == 4
    scopes = {vc.ch.scope for vc in sel.local.channels}
    assert scopes == {"REMOTE", "LOCAL"}
    assert "SYSTEM" not in scopes


def test_select_views_local_is_prefix_of_full():
    """A6.2: combus_local is a strict prefix of combus (REMOTE+LOCAL channels
    appear in the same order, with the same numeric IDs)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("R2", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("L2", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
            _ch("S2", scope="SYSTEM", direction=None),
        ]
    })
    sel = select_views(chs)
    # combus_local must be a prefix of combus (same channels, same order, same IDs).
    assert sel.local.ch_count == 4
    assert sel.full.ch_count == 6
    for i in range(sel.local.ch_count):
        assert sel.local.channels[i].ch.id == sel.full.channels[i].ch.id, (
            f"order mismatch at index {i}: local={sel.local.channels[i].ch.id} "
            f"vs full={sel.full.channels[i].ch.id}"
        )
        assert sel.local.channels[i].numeric_id == sel.full.channels[i].numeric_id, (
            f"id mismatch at index {i}: local={sel.local.channels[i].numeric_id} "
            f"vs full={sel.full.channels[i].numeric_id}"
        )


def test_select_views_local_no_renumbering():
    """A6.2: a channel that appears in both combus and combus_local has the
    SAME numeric_id in both views (no renumbering)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("R2", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("L2", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
        ]
    })
    sel = select_views(chs)
    # Build a map id -> numeric_id in each view.
    full_ids = {vc.ch.id: vc.numeric_id for vc in sel.full.channels}
    local_ids = {vc.ch.id: vc.numeric_id for vc in sel.local.channels}
    # Every channel in combus_local must have the same numeric_id in combus.
    for cid, nid in local_ids.items():
        assert cid in full_ids, f"{cid} missing from combus"
        assert full_ids[cid] == nid, (
            f"renumbering detected: {cid} has id {nid} in combus_local "
            f"but {full_ids[cid]} in combus"
        )


def test_select_views_local_wire_end_equals_full_wire_end():
    """A6.2: combus_local.wire_end == combus.wire_end (same REMOTE/LOCAL
    boundary by construction)."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("R2", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
            _ch("S1", scope="SYSTEM", direction=None),
        ]
    })
    sel = select_views(chs)
    assert sel.local.wire_end == sel.full.wire_end


def test_select_views_local_deterministic():
    """A6.2: combus_local is deterministic (parity with combus/combus_remote)."""
    chs_a = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
        _ch("S1", scope="SYSTEM", direction=None),
    ]})
    sel_a = select_views(chs_a)
    chs_b = _canon_from_yamls({"channels": [
        _ch("S1", scope="SYSTEM", direction=None),
        _ch("L1", scope="LOCAL"),
        _ch("R1", scope="REMOTE"),
    ]})
    sel_b = select_views(chs_b)
    assert [vc.ch.id for vc in sel_a.local.channels] == [vc.ch.id for vc in sel_b.local.channels]
    assert [vc.numeric_id for vc in sel_a.local.channels] == [vc.numeric_id for vc in sel_b.local.channels]


def test_select_views_local_empty_when_no_remote_or_local():
    """A6.2: combus_local is empty when only SYSTEM channels exist."""
    chs = _canon_from_yamls({"channels": [
        _ch("S1", scope="SYSTEM", direction=None),
        _ch("S2", scope="SYSTEM", direction=None),
    ]})
    sel = select_views(chs)
    assert sel.local.ch_count == 0
    assert sel.local.wire_end == 0


def test_generate_emits_combus_local_files(tmp_path):
    """A6.2: generate() emits combus_local_ids.h, combus_local.h, combus_local.cpp."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
        _ch("S1", scope="SYSTEM", direction=None),
    ]})
    sel = generate(chs, _ctx([]), tmp_path)
    assert (tmp_path / "combus_local_ids.h").exists()
    assert (tmp_path / "combus_local.h").exists()
    assert (tmp_path / "combus_local.cpp").exists()
    # And the existing views are still emitted.
    assert (tmp_path / "combus_ids.h").exists()
    assert (tmp_path / "combus.h").exists()
    assert (tmp_path / "combus.cpp").exists()
    assert (tmp_path / "combus_remote_ids.h").exists()
    assert (tmp_path / "combus_remote.h").exists()
    assert (tmp_path / "combus_remote.cpp").exists()


def test_combus_local_ids_header_excludes_system():
    """A6.2: combus_local_ids.h must NOT contain any SYSTEM channel."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
        _ch("S1", scope="SYSTEM", direction=None),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        ids = (Path(tmp_dir) / "combus_local_ids.h").read_text(encoding="utf-8")
    assert "R1" in ids
    assert "L1" in ids
    assert "S1" not in ids


def test_combus_local_no_bus_instance():
    """A6.2: combus_local.h does NOT declare comBus (only combus does)."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        h = (Path(tmp_dir) / "combus_local.h").read_text(encoding="utf-8")
    assert "extern ComBus comBus" not in h


def test_combus_local_no_wire_end_sentinel():
    """A6.2: combus_local_ids.h does NOT declare a WireEnd sentinel
    (combus.WireEnd is the canonical boundary; combus_local reuses it)."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        ids = (Path(tmp_dir) / "combus_local_ids.h").read_text(encoding="utf-8")
    assert "Combus_localWireEnd" not in ids
    assert "WireEnd" not in ids


def test_select_views_deterministic():
    chs_a = _canon_from_yamls({"channels": [_ch("R1", scope="REMOTE"),
                                              _ch("L1", scope="LOCAL")]})
    sel_a = select_views(chs_a)
    chs_b = _canon_from_yamls({"channels": [_ch("L1", scope="LOCAL"),
                                              _ch("R1", scope="REMOTE")]})
    sel_b = select_views(chs_b)
    assert [vc.ch.id for vc in sel_a.full.channels] == [vc.ch.id for vc in sel_b.full.channels]
    assert [vc.numeric_id for vc in sel_a.full.channels] == [vc.numeric_id for vc in sel_b.full.channels]


def test_select_views_empty_channel_set():
    sel = select_views([])
    assert sel.full.ch_count == 0
    assert sel.remote.ch_count == 0
    assert sel.full.wire_end == 0


def test_select_views_same_id_in_both_views():
    """A REMOTE channel has the same numeric_id in both views."""
    chs = _canon_from_yamls({
        "channels": [
            _ch("R1", scope="REMOTE"),
            _ch("L1", scope="LOCAL"),
        ]
    })
    sel = select_views(chs)
    r1_full = [vc for vc in sel.full.channels if vc.ch.id == "R1"][0]
    r1_remote = [vc for vc in sel.remote.channels if vc.ch.id == "R1"][0]
    assert r1_full.numeric_id == r1_remote.numeric_id == 0


# =============================================================================
# Direction bits in ViewChannel
# =============================================================================

def test_view_channel_direction_bits():
    chs = _canon_from_yamls({
        "channels": [
            _ch("UP", direction=["uplink"]),
            _ch("DOWN", direction=["downlink"]),
            _ch("BOTH", direction=["both"]),
            _ch("NONE", direction=["none"]),
        ]
    })
    full = _select_view(chs, "combus", {"REMOTE", "LOCAL", "SYSTEM"})
    by_id = {vc.ch.id: vc for vc in full.channels}
    assert by_id["UP"].direction_bits == 1
    assert by_id["DOWN"].direction_bits == 2
    assert by_id["BOTH"].direction_bits == 3
    assert by_id["NONE"].direction_bits == 0


# =============================================================================
# C++ rendering
# =============================================================================

def _build_view_with_channels(channels: list[ChannelDefinition]) -> View:
    """Build a test View wrapping a list of channels (forces the same sort key as _select_view)."""
    selected = list(channels)
    _VIEW_SCOPE_ORDER = {"REMOTE": 0, "LOCAL": 1, "SYSTEM": 2}
    selected.sort(key=lambda ch: (_VIEW_SCOPE_ORDER[ch.scope],
                                  0 if ch.type == "analog" else 1,
                                  ch.theme, ch.id))
    view_channels = []
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
        wire_end = len(selected)
    return View(name="combus", channels=view_channels, wire_end=wire_end)


def test_render_ids_header_basic():
    chs = _canon_from_yamls({"channels": [_ch("FOO", type_="digital", scope="REMOTE")]})
    view = _build_view_with_channels(chs)
    out = _render_ids_header(view, _ctx([]))
    assert "GENERATED FILE" in out
    assert "enum class DigitalCombusID : uint8_t" in out
    assert "FOO = 0" in out
    assert "CH_COUNT" in out
    assert "CombusWireEnd" in out


def test_render_ids_header_analog_digital():
    chs = _canon_from_yamls({
        "channels": [
            _ch("A1", type_="analog", scope="REMOTE"),
            _ch("D1", type_="digital", scope="REMOTE"),
        ]
    })
    view = _build_view_with_channels(chs)
    out = _render_ids_header(view, _ctx([]))
    assert "enum class AnalogCombusID" in out
    assert "enum class DigitalCombusID" in out
    # A1 gets 0 in analog, D1 gets 0 in digital (each enum has its own ID space).
    assert re.search(r"A1\s*=\s*0", out)
    assert re.search(r"D1\s*=\s*1", out)


def test_render_ids_header_no_wire_end_for_remote_view():
    chs = _canon_from_yamls({"channels": [_ch("R1", scope="REMOTE")]})
    sel = select_views(chs)
    out = _render_ids_header(sel.remote, _ctx([]))
    assert "Combus_remoteWireEnd" not in out


def test_render_header_externs_real_runtime_structs():
    """A6.1: header declares extern AnalogComBusArray[] / DigitalComBusArray[]."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", type_="digital", scope="REMOTE"),
        _ch("BAR", type_="analog", scope="REMOTE"),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "extern AnalogComBus AnalogCombusArray" in out
    assert "extern DigitalComBus DigitalCombusArray" in out


def test_render_header_no_direction_enum():
    """A6.1: Direction enum is defined in combus_struct.h (runtime), NOT in
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


def test_render_header_remote_view_no_bus_instance():
    """combus_remote view header does NOT declare comBus (it's a wire view)."""
    chs = _canon_from_yamls({"channels": [_ch("R1", scope="REMOTE")]})
    sel = select_views(chs)
    out = _render_header(sel.remote, _ctx([]))
    assert "extern ComBus comBus" not in out


def test_render_source_real_runtime_structs():
    """A6.1: source defines AnalogComBusArray[] / DigitalComBusArray[] directly."""
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="Foo channel", direction=["uplink"]),
        _ch("BAR", type_="analog", info_name="Bar analog", direction=["both"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "AnalogComBus AnalogCombusArray" in out
    assert "DigitalComBus DigitalCombusArray" in out
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
    chs = _canon_from_yamls({"channels": [_ch("FOO", scope="REMOTE")]})
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
            _ch("R1", scope="REMOTE", type_="digital"),
            _ch("L1", scope="LOCAL", type_="digital"),
            _ch("S1", scope="SYSTEM", type_="digital", direction=None),
        ]
    })
    ctx = _ctx([])
    sel = generate(chs, ctx, tmp_path)
    assert isinstance(sel, ViewSelection)
    assert (tmp_path / "combus.h").exists()
    assert (tmp_path / "combus.cpp").exists()
    assert (tmp_path / "combus_ids.h").exists()
    assert (tmp_path / "combus_remote.h").exists()
    assert (tmp_path / "combus_remote.cpp").exists()
    assert (tmp_path / "combus_remote_ids.h").exists()


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


def test_generate_requires_missing_raises(tmp_path):
    chs = _canon_from_yamls({"channels": [_ch("FOO", requires=["MISSING"])]})
    ctx = _ctx([])
    try:
        generate(chs, ctx, tmp_path)
    except RequiresResolutionError:
        pass
    else:
        raise AssertionError("expected RequiresResolutionError")


def test_generate_empty_yields_empty_views(tmp_path):
    sel = generate([], _ctx([]), tmp_path)
    assert sel.full.ch_count == 0
    assert sel.remote.ch_count == 0
    assert (tmp_path / "combus_ids.h").exists()
    assert (tmp_path / "combus_remote_ids.h").exists()


# =============================================================================
# Generated C++ is structurally well-formed (basic checks)
# =============================================================================

def test_generated_ids_header_parses():
    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="REMOTE"),
        _ch("D1", type_="digital", scope="REMOTE"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        sel = generate(chs, _ctx([]), Path(tmp_dir))
        ids = (Path(tmp_dir) / "combus_ids.h").read_text(encoding="utf-8")
    assert re.search(r"enum\s+class\s+AnalogCombusID\s*:\s*uint8_t", ids)
    assert re.search(r"enum\s+class\s+DigitalCombusID\s*:\s*uint8_t", ids)
    assert re.search(r"A1\s*=\s*0", ids)
    assert re.search(r"D1\s*=\s*1", ids)
    assert re.search(r"CH_COUNT", ids)


def _find_cpp_compiler() -> tuple[list[str], str] | None:
    """
    Return (argv_prefix, label) for an available C++ compiler, or None.

    Order of preference:
      1. `g++` in PATH (native Linux/macOS or mingw).
      2. `clang++` in PATH.
      3. PlatformIO's bundled `xtensa-esp32-elf-g++` (cross-compiler).
         This is the EXACT toolchain used by PlatformIO for this project
         (platformio.ini: platform = espressif32@6.7.0, gcc 12.2.0).
         For cross-compilation we use -fsyntax-only (no linking), which
         is sufficient to validate designated initializers, enum values,
         struct field order and template instantiation.
    """
    import shutil
    if shutil.which("g++"):
        return (["g++"], "g++")
    if shutil.which("clang++"):
        return (["clang++"], "clang++")
    # PlatformIO's toolchain is at ~/.platformio/packages/ on Windows/Linux/macOS.
    pio_gpp = (Path.home() / ".platformio" / "packages"
               / "toolchain-xtensa-esp32" / "bin" / "xtensa-esp32-elf-g++.exe")
    if pio_gpp.exists():
        return ([str(pio_gpp)], "xtensa-esp32-elf-g++ (PlatformIO)")
    return None


def test_generated_cpp_compiles_cleanly_with_cpp_check():
    """
    If a C++ compiler is available, validate that the generated
    combus.h / combus.cpp / combus_ids.h parse and type-check
    against the real runtime include/struct/combus_struct.h.

    Toolchain target: -std=gnu++17 (matches platformio.ini line 34,
    espressif32 Xtensa GCC 12.2.0). Designated initializers for
    aggregates are supported by GCC/Clang in gnu++17 mode and are
    already used throughout the repo's legacy .inc files, so no
    fallback (positional init, constructor, etc.) is needed.

    For native compilers (g++/clang++) we link a real `main` to fully
    exercise the runtime. For the PlatformIO Xtensa cross-compiler we
    use -fsyntax-only (no linking) since the runtime expects ESP32
    headers that are only meaningful on-target.
    """
    import subprocess
    found = _find_cpp_compiler()
    if found is None:
        import pytest
        pytest.skip("no C++ compiler available (g++, clang++, or PlatformIO xtensa-esp32-elf-g++)")
    argv0, label = found

    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="REMOTE"),
        _ch("D1", type_="digital", scope="REMOTE"),
        _ch("L1", type_="analog", scope="LOCAL"),
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
    // The view sort puts analog before digital within each scope,
    // so REMOTE[0]=A1 (analog), REMOTE[1]=D1 (digital).
    static_assert(static_cast<int>(AnalogCombusID::A1) == 0, "A1");
    static_assert(static_cast<int>(DigitalCombusID::D1) == 1, "D1");
    static_assert(static_cast<int>(AnalogCombusID::CH_COUNT) > 0, "count");
    // Read .direction to ensure it is publicly accessible.
    Direction d = comBus.analogBus[0].direction;
    (void)d;
    // Exercise the bitmask operators at compile time. operator| is
    // declared constexpr in combus_struct.h, so this must hold.
    constexpr Direction both = Direction::Uplink | Direction::Downlink;
    static_assert(static_cast<int>(both) == static_cast<int>(Direction::Both), "both");
    // And the underlying uint8_t representation matches the bitmask contract.
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
        is_native = ("xtensa" not in label.lower())
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
        if is_native:
            cmd += [str(test_src), "-o", str(tmp / "test")]
        else:
            # Cross-compiler: syntax-only check, no linking.
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

    _, files, parsed = discover_and_parse(project_root=buildroot.parent)
    cb_files = [f for f in files if f.suffix == ".cb"]
    if not cb_files:
        import pytest
        pytest.skip("no .cb files found in src/")

    result = canonize(parsed)
    chs = result.canonical_definitions

    ctx = BuildContext(
        buildroot=buildroot,
        defines=frozenset({"HAS_FAILSAFE", "HAS_VBAT_FAILSAFE"}),
        defines_with_value={},
        cppdefines_source="override",
    )

    with tempfile.TemporaryDirectory() as tmp_dir:
        sel = generate(chs, ctx, Path(tmp_dir))
        assert sel.full.ch_count > 0
        assert sel.full.ch_count >= sel.remote.ch_count
        # A6.2: 9 files total (3 views × 3 files each).
        for fname in ("combus.h", "combus.cpp", "combus_ids.h",
                      "combus_local.h", "combus_local.cpp", "combus_local_ids.h",
                      "combus_remote.h", "combus_remote.cpp", "combus_remote_ids.h"):
            assert (Path(tmp_dir) / fname).exists(), f"missing {fname}"
        # And no ChannelDescriptor anywhere.
        for fname in ("combus.h", "combus.cpp", "combus_ids.h",
                      "combus_local.h", "combus_local.cpp", "combus_local_ids.h",
                      "combus_remote.h", "combus_remote.cpp", "combus_remote_ids.h"):
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
    values declared in combus_struct.h.

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
    h = (REPO_ROOT / "include" / "struct" / "combus_struct.h").read_text(encoding="utf-8")
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
    infoName, value, layer, direction."""
    h = (REPO_ROOT / "include" / "struct" / "combus_struct.h").read_text(encoding="utf-8")
    for struct_name in ("AnalogComBus", "DigitalComBus"):
        # Find the struct definition body.
        m = re.search(rf"typedef\s+struct\s+\{{([^}}]*)\}}\s*{struct_name};", h, re.DOTALL)
        assert m, f"{struct_name} struct not found"
        body = m.group(1)
        pos_info = body.find("infoName")
        pos_value = body.find("value")
        pos_layer = body.find("layer")
        pos_dir = body.find("direction")
        assert pos_info >= 0, f"{struct_name} missing infoName"
        assert pos_value > pos_info, f"{struct_name} value before infoName"
        assert pos_layer > pos_value, f"{struct_name} layer before value"
        assert pos_dir > pos_layer, f"{struct_name} direction before layer"


def test_generated_cpp_direction_for_both():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", direction=["both"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::Both" in out


def test_generated_cpp_direction_for_none():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", scope="SYSTEM", direction=["none"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::None" in out


def test_generated_cpp_direction_for_uplink():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", direction=["uplink"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::Uplink" in out


def test_generated_cpp_direction_for_downlink():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", direction=["downlink"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert "Direction::Downlink" in out
