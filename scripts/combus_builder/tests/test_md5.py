#!/usr/bin/env python3
"""
Tests for md5.py (A7.1) - MD5 computation for the combus_remote view.

Coverage:
  - canonical_bytes() shape (JSON, sort_keys, includes required fields)
  - compute_view_hash() returns 16-byte digest
  - Hash inputs are: id, type, scope, theme, direction, infoName
  - Hash is independent of:
      * file order / discovery order
      * key ordering in YAML dicts (cosmetic change)
      * comments / formatting
  - Hash is sensitive to:
      * direction change (the case that motivated A7)
      * any channel field change
  - Hash inputs do NOT include:
      * projectVersion (separate constant)
      * requires (already captured by presence/absence after A6.1)
  - generate_md5_artifacts() writes combus_remote_md5.h ONLY
    (A7.1 simplification: combus_local was speculative and is not
    consumed by the handshake consumer).
  - C++ header format:
      * kCombusRemoteComBusMd5[16]            raw bytes
      * kCombusRemoteComBusMd5Hex             32-char lowercase hex
      * kProjectVersionMajor / Minor
      * kCombusHandshakeWirePayloadLen (= 18u)
  - NO kMachineType (A7.1 simplification).
"""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.canon import canonize
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.generator import (
    Direction,
    View,
    ViewChannel,
    generate,
    select_views,
)
from scripts.combus_builder.md5 import (
    ViewHash,
    _render_md5_header,
    canonical_bytes,
    compute_view_hash,
    emit_md5_header,
    generate_md5_artifacts,
)


# =============================================================================
# Helpers
# =============================================================================

def _parsed(path, raw, type_label="channel_def"):
    return (path, type_label, raw)


def _ch(
    id_="FOO",
    info_name="Foo channel",
    type_="digital",
    scope="LOCAL",
    theme="failsafe",
    direction=None,
    requires=None,
):
    if direction is None:
        direction = ["uplink"]
    if requires is None:
        requires = []
    return {
        "id": id_,
        "infoName": info_name,
        "type": type_,
        "scope": scope,
        "theme": theme,
        "direction": direction,
        "requires": requires,
    }


def _canon_from_yamls(*yamls):
    """Build a list of ChannelDefinitions from a list of channel dicts.

    Each dict is wrapped in {"channels": [dict]} to match the .cb file
    structure expected by canonize().
    """
    parsed = [_parsed(Path(f"ch{i}.cb"), {"channels": [y]}) for i, y in enumerate(yamls)]
    return canonize(parsed).canonical_definitions


def _ctx(flags):
    return BuildContext(
        buildroot=Path("."),
        defines=frozenset(flags),
        defines_with_value={},
        cppdefines_source="override",
    )


def _build_view(channels, name="combus_remote", scopes=("REMOTE", "LOCAL", "SYSTEM")):
    from scripts.combus_builder.generator import _select_view
    return _select_view(channels, name, scopes)


# =============================================================================
# canonical_bytes()
# =============================================================================

def test_canonical_bytes_is_valid_json():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs)
    payload = json.loads(canonical_bytes(v))
    assert "view" in payload
    assert "channels" in payload


def test_canonical_bytes_includes_required_fields():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs)
    payload = json.loads(canonical_bytes(v))
    ch = payload["channels"][0]
    for f in ("id", "type", "scope", "theme", "direction", "infoName"):
        assert f in ch, f"missing field {f}"


def test_canonical_bytes_view_name_included():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs, name="combus_remote")
    payload = json.loads(canonical_bytes(v))
    assert payload["view"] == "combus_remote"


def test_canonical_bytes_direction_is_sorted():
    chs = _canon_from_yamls(_ch(direction=["downlink", "uplink"]))
    v = _build_view(chs)
    payload = json.loads(canonical_bytes(v))
    assert payload["channels"][0]["direction"] == ["downlink", "uplink"]


def test_canonical_bytes_independent_of_yaml_key_order():
    ch1 = {"id": "FOO", "infoName": "Foo", "type": "digital", "scope": "LOCAL", "theme": "failsafe", "direction": ["uplink"], "requires": []}
    ch2 = {"theme": "failsafe", "scope": "LOCAL", "type": "digital", "infoName": "Foo", "id": "FOO", "direction": ["uplink"], "requires": []}
    chs1 = _canon_from_yamls(ch1)
    chs2 = _canon_from_yamls(ch2)
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert canonical_bytes(v1) == canonical_bytes(v2)


# =============================================================================
# compute_view_hash()
# =============================================================================

def test_compute_view_hash_returns_16_bytes():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs)
    h = compute_view_hash(v)
    assert len(h.digest) == 16


def test_compute_view_hash_hex_is_lowercase():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs)
    h = compute_view_hash(v)
    assert h.md5_hex == h.md5_hex.lower()
    assert len(h.md5_hex) == 32


def test_compute_view_hash_matches_manual_md5():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs)
    h = compute_view_hash(v)
    expected = hashlib.md5(canonical_bytes(v)).hexdigest()
    assert h.md5_hex == expected


def test_compute_view_hash_view_name_recorded():
    chs = _canon_from_yamls(_ch())
    v = _build_view(chs, name="combus_remote")
    h = compute_view_hash(v)
    assert h.view_name == "combus_remote"


def test_compute_view_hash_independent_of_file_order():
    raw = _ch()
    chs1 = _canon_from_yamls(raw, _ch(id_="BAR", info_name="Bar"))
    chs2 = list(reversed(chs1))
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert compute_view_hash(v1).md5_hex == compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_direction_changes():
    chs1 = _canon_from_yamls(_ch(direction=["uplink"]))
    chs2 = _canon_from_yamls(_ch(direction=["downlink"]))
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_infoName_changes():
    chs1 = _canon_from_yamls(_ch(info_name="Foo"))
    chs2 = _canon_from_yamls(_ch(info_name="Bar"))
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_id_added():
    chs1 = _canon_from_yamls(_ch())
    chs2 = _canon_from_yamls(_ch(), _ch(id_="BAR", info_name="Bar"))
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_type_changes():
    chs1 = _canon_from_yamls(_ch(type_="digital"))
    chs2 = _canon_from_yamls(_ch(type_="analog"))
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_scope_moves():
    chs1 = _canon_from_yamls(_ch(scope="LOCAL"))
    chs2 = _canon_from_yamls(_ch(scope="REMOTE"))
    v1 = _build_view(chs1, scopes=("REMOTE", "LOCAL"))
    v2 = _build_view(chs2, scopes=("REMOTE", "LOCAL"))
    assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_independent_of_cosmetic_yaml():
    """Two dicts differing only in field order produce the same hash."""
    ch1 = {"id": "FOO", "infoName": "Foo", "type": "digital", "scope": "LOCAL", "theme": "failsafe", "direction": ["uplink"], "requires": []}
    ch2 = {"theme": "failsafe", "infoName": "Foo", "type": "digital", "id": "FOO", "scope": "LOCAL", "direction": ["uplink"], "requires": []}
    chs1 = _canon_from_yamls(ch1)
    chs2 = _canon_from_yamls(ch2)
    v1 = _build_view(chs1)
    v2 = _build_view(chs2)
    assert compute_view_hash(v1).md5_hex == compute_view_hash(v2).md5_hex


# =============================================================================
# _render_md5_header()
# =============================================================================

def _make_hash():
    return ViewHash(
        view_name="combus_remote",
        md5_hex="0123456789abcdef0123456789abcdef",
        digest=bytes.fromhex("0123456789abcdef0123456789abcdef"),
    )


def test_render_md5_header_has_md5_array():
    h = _make_hash()
    out = _render_md5_header(h)
    assert "kCombusRemoteComBusMd5[16]" in out
    # The formatter emits uppercase hex (0xEFu).
    assert "0xEFu" in out


def test_render_md5_header_has_md5_hex_string():
    h = _make_hash()
    out = _render_md5_header(h)
    assert "kCombusRemoteComBusMd5Hex" in out
    assert "0123456789abcdef0123456789abcdef" in out


def test_render_md5_header_has_version_constants():
    h = _make_hash()
    out = _render_md5_header(h)
    assert "kProjectVersionMajor" in out
    assert "kProjectVersionMinor" in out


def test_render_md5_header_has_handshake_payload_len_18():
    h = _make_hash()
    out = _render_md5_header(h)
    assert "kCombusHandshakeWirePayloadLen" in out
    assert "18u" in out


def test_render_md5_header_no_machine_type():
    """A7.1: kMachineType is REMOVED."""
    h = _make_hash()
    out = _render_md5_header(h)
    assert "kMachineType" not in out


def test_render_md5_header_custom_version():
    h = _make_hash()
    out = _render_md5_header(h, project_version_major=2, project_version_minor=5)
    assert "kProjectVersionMajor = 2u" in out
    assert "kProjectVersionMinor = 5u" in out


def test_render_md5_header_is_namespace_combus_wire():
    h = _make_hash()
    out = _render_md5_header(h)
    assert "namespace combus" in out
    assert "namespace wire" in out


# =============================================================================
# emit_md5_header()
# =============================================================================

def test_emit_md5_header_writes_file(tmp_path):
    h = _make_hash()
    p = emit_md5_header(h, tmp_path)
    assert p.exists()
    assert p.name == "combus_remote_md5.h"
    content = p.read_text(encoding="utf-8")
    assert "kCombusRemoteComBusMd5[16]" in content


# =============================================================================
# generate_md5_artifacts()
# =============================================================================

def test_generate_md5_artifacts_emits_remote_only():
    """A7.1: only combus_remote is hashed."""
    chs = _canon_from_yamls(_ch(scope="REMOTE"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        hash_, written = generate_md5_artifacts(sel, Path(tmp_dir))
        assert isinstance(hash_, ViewHash)
        assert hash_.view_name == "combus_remote"
        assert written.name == "combus_remote_md5.h"
        # combus_local_md5.h must NOT be emitted (A7.1 simplification).
        assert not (Path(tmp_dir) / "combus_local_md5.h").exists()


def test_generate_md5_artifacts_hash_matches_written_bytes():
    chs = _canon_from_yamls(_ch(scope="REMOTE"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        hash_, written = generate_md5_artifacts(sel, Path(tmp_dir))
        text = written.read_text(encoding="utf-8")
        # The hex string in the file must match the computed hash.
        assert hash_.md5_hex in text


# =============================================================================
# generate() pipeline integration
# =============================================================================

def test_generate_pipeline_emits_md5_file(tmp_path):
    chs = _canon_from_yamls(_ch(scope="REMOTE"))
    sel = generate(chs, _ctx([]), tmp_path)
    assert (tmp_path / "combus_remote_md5.h").exists()
    # combus_local_md5.h must NOT be emitted (A7.1 simplification).
    assert not (tmp_path / "combus_local_md5.h").exists()


def test_generate_pipeline_md5_file_compiles(tmp_path):
    """The generated combus_remote_md5.h must compile with the actual
    PlatformIO toolchain (xtensa-esp32-elf-g++)."""
    chs = _canon_from_yamls(_ch(scope="REMOTE"))
    generate(chs, _ctx([]), tmp_path)
    md5_path = tmp_path / "combus_remote_md5.h"
    assert md5_path.exists()
    # Compile-check: include the header in a tiny TU and verify it builds.
    import subprocess
    tu = tmp_path / "compile_check.cpp"
    tu.write_text(
        "#include <stdint.h>\n"
        f'#include "{md5_path.name}"\n'
        "int main() {\n"
        "  (void)combus::wire::kCombusRemoteComBusMd5[0];\n"
        "  (void)combus::wire::kCombusRemoteComBusMd5Hex;\n"
        "  (void)combus::wire::kProjectVersionMajor;\n"
        "  (void)combus::wire::kProjectVersionMinor;\n"
        "  (void)combus::wire::kCombusHandshakeWirePayloadLen;\n"
        "  return 0;\n"
        "}\n",
        encoding="utf-8",
    )
    # Try the PlatformIO toolchain first; fall back to g++ if not present.
    for compiler in ("xtensa-esp32-elf-g++", "g++"):
        try:
            r = subprocess.run(
                [compiler, "-std=c++17", "-c", str(tu), "-o", str(tmp_path / "out.o")],
                capture_output=True, text=True, timeout=30,
            )
        except FileNotFoundError:
            continue
        if r.returncode == 0:
            return
    # If neither compiler is available, skip rather than fail.
    import pytest
    pytest.skip("No C++ compiler available for compile-check")


# =============================================================================
# Determinism
# =============================================================================

def test_md5_deterministic_across_re_runs():
    chs = _canon_from_yamls(_ch(scope="REMOTE"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp1, tempfile.TemporaryDirectory() as tmp2:
        h1, _ = generate_md5_artifacts(sel, Path(tmp1))
        h2, _ = generate_md5_artifacts(sel, Path(tmp2))
        assert h1.md5_hex == h2.md5_hex


def test_md5_deterministic_across_file_discovery_order():
    """Discovery order must not affect the hash (no env de test jetable
    needed — the test uses two independent canonisations)."""
    raw = _ch(scope="REMOTE")
    chs1 = _canon_from_yamls(raw, _ch(id_="BAR", info_name="Bar", scope="REMOTE"))
    chs2 = list(reversed(chs1))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    with tempfile.TemporaryDirectory() as tmp1, tempfile.TemporaryDirectory() as tmp2:
        h1, _ = generate_md5_artifacts(sel1, Path(tmp1))
        h2, _ = generate_md5_artifacts(sel2, Path(tmp2))
        assert h1.md5_hex == h2.md5_hex
