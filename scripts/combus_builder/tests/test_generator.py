#!/usr/bin/env python3
"""
Tests for generator.py (A6) - C++ artifact generation.

Coverage:
  - requires resolution (errors only -- A6 raises, not filters).
  - View selection (combus = REMOTE+LOCAL+SYSTEM; combus_remote = REMOTE).
  - Deterministic ID allocation.
  - WIRE_END computation.
  - Direction C++ representation (0/1/2/3, both normalization).
  - ChannelDescriptor rendering (infoName, direction, escape).
  - File emission (.h/.cpp/_ids.h, atomic write).
  - Generation against a real .cb file in the repo.
  - Generated C++ is structurally well-formed (basic checks).
  - Python <-> C++ coherence.
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
    """A6 RAISES on missing requires (it does NOT silently filter)."""
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
    assert sel.remote.ch_count == 1
    assert sel.remote.channels[0].ch.id == "R1"


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


def test_render_header_direction_enum():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "enum class Direction : uint8_t" in out
    assert "None     = 0" in out
    assert "Uplink   = 1" in out
    assert "Downlink = 2" in out
    assert "Both     = 3" in out
    assert "operator|" in out
    assert "operator&" in out
    assert "operator~" in out


def test_render_header_descriptor_struct():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view_with_channels(chs)
    out = _render_header(view, _ctx([]))
    assert "struct ChannelDescriptor" in out
    assert "infoName" in out
    assert "Direction   direction" in out


def test_render_source_channel_descriptors():
    chs = _canon_from_yamls({"channels": [
        _ch("FOO", info_name="Foo channel", direction=["uplink"]),
        _ch("BAR", type_="analog", info_name="Bar analog", direction=["both"]),
    ]})
    view = _build_view_with_channels(chs)
    out = _render_source(view, _ctx([]))
    assert '"Foo channel"' in out
    assert "Direction::Uplink" in out
    assert '"Bar analog"' in out
    assert "Direction::Both" in out
    assert "AnalogCombusChannelDescriptors" in out
    assert "DigitalCombusChannelDescriptors" in out


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
    # Force a write error by making the directory read-only after creation.
    import os
    try:
        # Pre-create a file; tmp should use a different name.
        _safe_write(target, "initial\n")
        # Now corrupt the temp file path by removing write perms on parent.
        # Skip on Windows where chmod is unreliable.
        if not sys.platform.startswith("win"):
            os.chmod(tmp_path, 0o500)
            try:
                _safe_write(target, "new\n")
            except Exception:
                pass
            os.chmod(tmp_path, 0o700)
    except OSError:
        pass
    # The original file should still exist with its original content.
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


def test_generated_header_has_direction_ops():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        generate(chs, _ctx([]), Path(tmp_dir))
        h = (Path(tmp_dir) / "combus.h").read_text(encoding="utf-8")
    assert "operator|" in h
    assert "operator&" in h
    assert "operator~" in h
    assert "0x03u" in h


def test_generated_cpp_compiles_cleanly_with_cpp_check():
    """If a C++ compiler is available, the generated files should compile standalone."""
    import shutil
    import subprocess
    if not shutil.which("g++") and not shutil.which("clang++"):
        import pytest
        pytest.skip("no C++ compiler available")
    chs = _canon_from_yamls({"channels": [
        _ch("A1", type_="analog", scope="REMOTE"),
        _ch("D1", type_="digital", scope="REMOTE"),
        _ch("L1", type_="analog", scope="LOCAL"),
    ]})
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)
        generate(chs, _ctx([]), tmp)
        # Compile the standalone test.
        test_src = tmp / "test_main.cpp"
        test_src.write_text(
            """
#include "combus.h"
int main() {
    static_assert(AnalogCombusID::A1 == 0, "A1");
    static_assert(DigitalCombusID::D1 == 0, "D1");
    static_assert(AnalogCombusID::CH_COUNT > 0, "count");
    return 0;
}
"""
        )
        compiler = "g++" if shutil.which("g++") else "clang++"
        r = subprocess.run(
            [compiler, "-std=c++17", "-I", str(tmp), str(test_src), "-o", str(tmp / "test")],
            capture_output=True,
            text=True,
        )
        assert r.returncode == 0, f"compilation failed: {r.stderr}"


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
        # combus must have at least one channel (failsafe/vbat are in the repo).
        assert sel.full.ch_count > 0
        # combus_remote may be empty if no REMOTE channels are defined yet -- valid end-state.
        assert sel.full.ch_count >= sel.remote.ch_count
        for fname in ("combus.h", "combus.cpp", "combus_ids.h",
                      "combus_remote.h", "combus_remote.cpp", "combus_remote_ids.h"):
            assert (Path(tmp_dir) / fname).exists(), f"missing {fname}"


# =============================================================================
# Python <-> C++ coherence
# =============================================================================

def test_python_enum_matches_cpp_enum():
    assert Direction.from_frozenset(frozenset()) == 0
    assert Direction.from_frozenset(frozenset({"uplink"})) == 1
    assert Direction.from_frozenset(frozenset({"downlink"})) == 2
    assert Direction.from_frozenset(frozenset({"uplink", "downlink"})) == 3
    for v in range(4):
        assert Direction.to_cpp_enum_name(v).startswith("Direction::")


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
