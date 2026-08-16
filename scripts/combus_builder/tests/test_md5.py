#!/usr/bin/env python3
"""
Tests for md5.py (A7) - MD5 computation for combus views.

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
      * machineType (separate constant)
      * projectVersion (separate constant)
      * requires (already captured by presence/absence after A8)
  - generate_md5_artifacts() writes <view>_md5.h files for combus_local and
    combus_remote (NOT for combus).
  - C++ header format (A7.1):
      * k<Cap>ComBusMd5[16]             raw bytes (per-view)
      * k<Cap>ComBusMd5Hex              32-char lowercase hex (per-view)
      * combus_local_md5.h also carries the SHARED constants:
          - kProjectVersionMajor / Minor
          - kCombusHandshakeWirePayloadLen (= 18u)
          - kMachineType
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
    compute_hashes,
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
    d = {
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


def _canon_from_yamls(*yamls):
    parsed = []
    for i, y in enumerate(yamls):
        p = Path(f"/tmp/test_md5_{i}.cb")
        parsed.append(_parsed(p, y))
    return canonize(parsed).canonical_definitions


def _ctx(flags):
    return BuildContext(
        buildroot=Path("/tmp"),
        defines=frozenset(flags),
        defines_with_value={},
        cppdefines_source="override",
    )


def _build_view(channels, name="combus_local", scopes=("REMOTE", "LOCAL")):
    """Build a View directly from a list of ChannelDefinitions (helper for unit tests)."""
    from scripts.combus_builder.generator import _VIEW_SCOPE_ORDER
    selected = [c for c in channels if c.scope in scopes]
    selected.sort(key=lambda c: (
        _VIEW_SCOPE_ORDER[c.scope],
        0 if c.type == "analog" else 1,
        c.theme,
        c.id,
    ))
    vcs = []
    wire_end = 0
    for idx, c in enumerate(selected):
        vcs.append(ViewChannel(
            numeric_id=idx,
            ch=c,
            direction_bits=Direction.from_frozenset(c.direction),
        ))
        if c.scope != "REMOTE" and wire_end == 0:
            wire_end = idx
    if wire_end == 0 and selected:
        wire_end = len(selected)
    return View(name=name, channels=vcs, wire_end=wire_end)


# =============================================================================
# canonical_bytes()
# =============================================================================

def test_canonical_bytes_is_valid_json():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view(chs)
    blob = canonical_bytes(view)
    parsed = json.loads(blob.decode("utf-8"))
    assert "view" in parsed
    assert "channels" in parsed
    assert isinstance(parsed["channels"], list)


def test_canonical_bytes_includes_required_fields():
    chs = _canon_from_yamls({"channels": [_ch("FOO", info_name="Hello")]})
    view = _build_view(chs)
    blob = json.loads(canonical_bytes(view).decode("utf-8"))
    ch0 = blob["channels"][0]
    for f in ("id", "type", "scope", "theme", "direction", "infoName"):
        assert f in ch0, f"missing field {f!r} in canonical channel dict"


def test_canonical_bytes_view_name_included():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view_local = _build_view(chs, name="combus_local")
    view_remote = _build_view(chs, name="combus_remote", scopes=("REMOTE",))
    blob_local = canonical_bytes(view_local).decode("utf-8")
    blob_remote = canonical_bytes(view_remote).decode("utf-8")
    assert '"view":"combus_local"' in blob_local
    assert '"view":"combus_remote"' in blob_remote


def test_canonical_bytes_direction_is_sorted():
    """direction frozenset is serialized as a sorted list — order independent."""
    chs_a = _canon_from_yamls({"channels": [_ch("FOO", direction=["uplink", "downlink"])]})
    chs_b = _canon_from_yamls({"channels": [_ch("FOO", direction=["downlink", "uplink"])]})
    view_a = _build_view(chs_a)
    view_b = _build_view(chs_b)
    assert canonical_bytes(view_a) == canonical_bytes(view_b)


def test_canonical_bytes_independent_of_yaml_key_order():
    """Cosmetic reordering of YAML keys must not affect the canonical blob."""
    raw1 = {"channels": [_ch("FOO", info_name="Hello")]}
    raw2 = {"channels": [{
        "direction": ["uplink"],
        "infoName": "Hello",
        "id": "FOO",
        "scope": "LOCAL",
        "theme": "failsafe",
        "type": "digital",
    }]}
    chs1 = _canon_from_yamls(raw1)
    chs2 = _canon_from_yamls(raw2)
    view1 = _build_view(chs1)
    view2 = _build_view(chs2)
    assert canonical_bytes(view1) == canonical_bytes(view2)


# =============================================================================
# compute_view_hash()
# =============================================================================

def test_compute_view_hash_returns_16_bytes():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view(chs)
    h = compute_view_hash(view)
    assert isinstance(h, ViewHash)
    assert len(h.digest) == 16
    assert len(h.md5_hex) == 32


def test_compute_view_hash_hex_is_lowercase():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view(chs)
    h = compute_view_hash(view)
    assert h.md5_hex == h.md5_hex.lower()
    assert h.md5_hex == h.digest.hex()


def test_compute_view_hash_matches_manual_md5():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view(chs)
    h = compute_view_hash(view)
    expected = hashlib.md5(canonical_bytes(view)).digest()
    assert h.digest == expected


def test_compute_view_hash_view_name_recorded():
    chs = _canon_from_yamls({"channels": [_ch("FOO")]})
    view = _build_view(chs, name="combus_local")
    h = compute_view_hash(view)
    assert h.view_name == "combus_local"


def test_compute_view_hash_independent_of_file_order():
    """Same channels in different YAML file order → identical hash."""
    chs1 = _canon_from_yamls(
        {"channels": [_ch("R1", scope="REMOTE"), _ch("L1", scope="LOCAL")]},
    )
    chs2 = _canon_from_yamls(
        {"channels": [_ch("L1", scope="LOCAL"), _ch("R1", scope="REMOTE")]},
    )
    view1 = _build_view(chs1)
    view2 = _build_view(chs2)
    h1 = compute_view_hash(view1)
    h2 = compute_view_hash(view2)
    assert h1.md5_hex == h2.md5_hex


def test_compute_view_hash_changes_when_direction_changes():
    """THE CASE THAT MOTIVATED A7: a direction-only change MUST change the hash."""
    chs_a = _canon_from_yamls({"channels": [
        _ch("FOO", scope="REMOTE", direction=["uplink"]),
    ]})
    chs_b = _canon_from_yamls({"channels": [
        _ch("FOO", scope="REMOTE", direction=["downlink"]),
    ]})
    view_a = _build_view(chs_a, scopes=("REMOTE",))
    view_b = _build_view(chs_b, scopes=("REMOTE",))
    assert compute_view_hash(view_a).md5_hex != compute_view_hash(view_b).md5_hex


def test_compute_view_hash_changes_when_infoName_changes():
    """Renaming a debug label must change the hash (infoName is in the input)."""
    chs_a = _canon_from_yamls({"channels": [
        _ch("FOO", scope="REMOTE", info_name="Original"),
    ]})
    chs_b = _canon_from_yamls({"channels": [
        _ch("FOO", scope="REMOTE", info_name="Renamed"),
    ]})
    view_a = _build_view(chs_a, scopes=("REMOTE",))
    view_b = _build_view(chs_b, scopes=("REMOTE",))
    assert compute_view_hash(view_a).md5_hex != compute_view_hash(view_b).md5_hex


def test_compute_view_hash_changes_when_id_added():
    chs_a = _canon_from_yamls({"channels": [
        _ch("FOO", scope="REMOTE"),
    ]})
    chs_b = _canon_from_yamls({"channels": [
        _ch("FOO", scope="REMOTE"),
        _ch("BAR", scope="REMOTE"),
    ]})
    view_a = _build_view(chs_a, scopes=("REMOTE",))
    view_b = _build_view(chs_b, scopes=("REMOTE",))
    assert compute_view_hash(view_a).md5_hex != compute_view_hash(view_b).md5_hex


def test_compute_view_hash_changes_when_type_changes():
    chs_a = _canon_from_yamls({"channels": [_ch("FOO", scope="REMOTE", type_="digital")]})
    chs_b = _canon_from_yamls({"channels": [_ch("FOO", scope="REMOTE", type_="analog")]})
    view_a = _build_view(chs_a, scopes=("REMOTE",))
    view_b = _build_view(chs_b, scopes=("REMOTE",))
    assert compute_view_hash(view_a).md5_hex != compute_view_hash(view_b).md5_hex


def test_compute_view_hash_changes_when_scope_moves():
    chs_a = _canon_from_yamls({"channels": [_ch("FOO", scope="REMOTE")]})
    chs_b = _canon_from_yamls({"channels": [_ch("FOO", scope="LOCAL")]})
    view_a = _build_view(chs_a, scopes=("REMOTE",))
    view_b = _build_view(chs_b, scopes=("LOCAL",))
    assert compute_view_hash(view_a).md5_hex != compute_view_hash(view_b).md5_hex


def test_compute_view_hash_independent_of_cosmetic_yaml():
    """Reordering keys, adding a comment in the YAML dict — hash stays the same."""
    raw_pretty = {
        "channels": [{
            "id": "FOO",
            "infoName": "Foo",
            "type": "digital",
            "scope": "REMOTE",
            "theme": "failsafe",
            "direction": ["uplink"],
        }]
    }
    raw_reordered = {
        "channels": [{
            "direction": ["uplink"],
            "theme": "failsafe",
            "scope": "REMOTE",
            "type": "digital",
            "infoName": "Foo",
            "id": "FOO",
        }]
    }
    chs_a = _canon_from_yamls(raw_pretty)
    chs_b = _canon_from_yamls(raw_reordered)
    view_a = _build_view(chs_a, scopes=("REMOTE",))
    view_b = _build_view(chs_b, scopes=("REMOTE",))
    assert compute_view_hash(view_a).md5_hex == compute_view_hash(view_b).md5_hex


def test_compute_hashes_returns_dict_keyed_by_view_name():
    chs = _canon_from_yamls({"channels": [_ch("R1", scope="REMOTE"), _ch("L1", scope="LOCAL")]})
    sel = select_views(chs)
    hashes = compute_hashes([sel.local, sel.remote])
    assert set(hashes.keys()) == {"combus_local", "combus_remote"}
    assert hashes["combus_local"].view_name == "combus_local"
    assert hashes["combus_remote"].view_name == "combus_remote"


# =============================================================================
# _render_md5_header() — C++ content checks
# =============================================================================

def _make_hash():
    return ViewHash(
        view_name="combus_local",
        md5_hex="0123456789abcdef0123456789abcdef",
        digest=bytes.fromhex("0123456789abcdef0123456789abcdef"),
    )


def test_render_md5_header_has_md5_array():
    h = _make_hash()
    out = _render_md5_header("combus_local", h)
    assert "kCombus_localComBusMd5[16]" in out
    assert "0x01u" in out
    assert "0xEFu" in out


def test_render_md5_header_has_md5_hex_string():
    h = _make_hash()
    out = _render_md5_header("combus_local", h)
    assert "kCombus_localComBusMd5Hex" in out
    assert "0123456789abcdef0123456789abcdef" in out


def test_render_md5_header_has_version_constants_in_local_only():
    """kProjectVersionMajor/Minor are emitted in combus_local_md5.h only."""
    h = _make_hash()
    out_local = _render_md5_header("combus_local", h)
    out_remote = _render_md5_header("combus_remote", h)
    assert "kProjectVersionMajor" in out_local
    assert "kProjectVersionMinor" in out_local
    assert "kProjectVersionMajor" not in out_remote
    assert "kProjectVersionMinor" not in out_remote


def test_render_md5_header_has_handshake_payload_len_18():
    """kCombusHandshakeWirePayloadLen = 18u (16 md5 + 2 version)."""
    h = _make_hash()
    out = _render_md5_header("combus_local", h)
    assert "kCombusHandshakeWirePayloadLen" in out
    assert "18u" in out


def test_render_md5_header_has_machine_type_string():
    h = _make_hash()
    out = _render_md5_header(
        "combus_local", h, machine_type="MACHINE_TYPE_DUMPER_TRUCK",
    )
    assert "kMachineType" in out
    assert "MACHINE_TYPE_DUMPER_TRUCK" in out


def test_render_md5_header_custom_machine_type():
    h = _make_hash()
    out = _render_md5_header(
        "combus_local", h, machine_type="MACHINE_TYPE_EXCAVATOR",
    )
    assert "MACHINE_TYPE_EXCAVATOR" in out


def test_render_md5_header_custom_version():
    h = _make_hash()
    out = _render_md5_header(
        "combus_local", h,
        project_version_major=2, project_version_minor=7,
    )
    assert "kProjectVersionMajor = 2u" in out
    assert "kProjectVersionMinor = 7u" in out


def test_render_md5_header_is_namespace_combus_wire():
    h = _make_hash()
    out = _render_md5_header("combus_local", h)
    assert "namespace combus" in out
    assert "namespace wire" in out


def test_render_md5_header_emits_shared_only_in_local():
    """A7.1: shared constants are emitted in combus_local_md5.h ONLY.

    The xtensa-esp32-elf-g++ toolchain used by PlatformIO rejects
    `inline constexpr` at namespace scope in some configurations, so we
    use the simpler pattern: combus_local_md5.h owns the shared
    constants; combus_remote_md5.h does not redefine them.
    """
    h = _make_hash()
    out_local = _render_md5_header("combus_local", h)
    out_remote = _render_md5_header("combus_remote", h)
    # Shared constants appear in combus_local_md5.h.
    assert "kProjectVersionMajor" in out_local
    assert "kMachineType" in out_local
    # ... and NOT in combus_remote_md5.h.
    assert "kProjectVersionMajor" not in out_remote
    assert "kProjectVersionMinor" not in out_remote
    assert "kMachineType" not in out_remote
    # The per-view MD5 is in both files (static constexpr, internal linkage).
    assert "kCombus_localComBusMd5" in out_local
    assert "kCombus_remoteComBusMd5" in out_remote
    # And it uses static constexpr (not inline constexpr — see A7.1).
    assert "static constexpr uint8_t kCombus_localComBusMd5" in out_local
    assert "static constexpr uint8_t kCombus_remoteComBusMd5" in out_remote


# =============================================================================
# emit_md5_header() — file emission (returns Path, writes to disk)
# =============================================================================

def test_emit_md5_header_writes_file(tmp_path):
    h = _make_hash()
    p = emit_md5_header("combus_local", h, tmp_path)
    assert isinstance(p, Path)
    assert p.name == "combus_local_md5.h"
    assert p.exists()
    on_disk = p.read_text(encoding="utf-8")
    assert on_disk == _render_md5_header("combus_local", h)


# =============================================================================
# generate_md5_artifacts() — file emission
# =============================================================================

def test_generate_md5_artifacts_emits_local_and_remote_only():
    """A7: combus (full) is intentionally NOT hashed."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
        _ch("S1", scope="SYSTEM", direction=None),
    ]})
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        hashes, written = generate_md5_artifacts(
            sel, Path(tmp_dir), machine_type="MACHINE_TYPE_DUMPER_TRUCK",
        )
        assert "combus_local" in written
        assert "combus_remote" in written
        assert (Path(tmp_dir) / "combus_local_md5.h").exists()
        assert (Path(tmp_dir) / "combus_remote_md5.h").exists()
        assert "combus" not in written
        assert not (Path(tmp_dir) / "combus_md5.h").exists()


def test_generate_md5_artifacts_hashes_match_written_bytes():
    """The MD5 emitted in the header matches the computed hash."""
    chs = _canon_from_yamls({"channels": [_ch("R1", scope="REMOTE")]})
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        hashes, written = generate_md5_artifacts(sel, Path(tmp_dir))
        local_h_text = (Path(tmp_dir) / "combus_local_md5.h").read_text(encoding="utf-8")
        assert hashes["combus_local"].md5_hex in local_h_text
        for b in hashes["combus_local"].digest:
            assert f"0x{b:02X}u" in local_h_text


def test_generate_pipeline_emits_md5_files(tmp_path):
    """The full generate() pipeline (A7) emits both md5 files."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
        _ch("S1", scope="SYSTEM", direction=None),
    ]})
    sel = generate(chs, _ctx([]), tmp_path)
    expected = [
        "combus_ids.h", "combus.h", "combus.cpp",
        "combus_local_ids.h", "combus_local.h", "combus_local.cpp",
        "combus_remote_ids.h", "combus_remote.h", "combus_remote.cpp",
        "combus_local_md5.h", "combus_remote_md5.h",
    ]
    for fname in expected:
        assert (tmp_path / fname).exists(), f"missing {fname}"


def test_generate_pipeline_md5_files_compile(tmp_path):
    """The generated md5 headers must be syntactically valid C++ even when
    both are included in the same TU (regression: shared constants must
    not be redefined)."""
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
    ]})
    generate(chs, _ctx([]), tmp_path)
    test_src = tmp_path / "test_md5_main.cpp"
    test_src.write_text(
        """
#include "combus_local_md5.h"
#include "combus_remote_md5.h"
#include <cstdint>

int main() {
    uint8_t a = combus::wire::kCombus_localComBusMd5[0];
    uint8_t b = combus::wire::kCombus_remoteComBusMd5[0];
    uint8_t v_maj = combus::wire::kProjectVersionMajor;
    uint8_t v_min = combus::wire::kProjectVersionMinor;
    uint8_t len = combus::wire::kCombusHandshakeWirePayloadLen;
    const char* mt = combus::wire::kMachineType;
    (void)a; (void)b; (void)v_maj; (void)v_min; (void)len; (void)mt;
    return 0;
}
"""
    )
    import subprocess, shutil
    found = None
    if shutil.which("g++"):
        found = (["g++"], "g++")
    elif shutil.which("clang++"):
        found = (["clang++"], "clang++")
    pio_gpp = (Path.home() / ".platformio" / "packages"
               / "toolchain-xtensa-esp32" / "bin" / "xtensa-esp32-elf-g++.exe")
    if found is None and pio_gpp.exists():
        found = ([str(pio_gpp)], "xtensa-esp32-elf-g++ (PlatformIO)")
    if found is None:
        import pytest
        pytest.skip("no C++ compiler available")
    argv0, label = found
    cmd = list(argv0) + [
        "-std=gnu++17",
        "-I", str(tmp_path),
        "-I", str(REPO_ROOT / "include"),
        str(test_src),
        "-o", str(tmp_path / "test_md5"),
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, (
        f"compilation failed with {label}: stdout={r.stdout!r}, stderr={r.stderr!r}"
    )


# =============================================================================
# _detect_machine_type() — generator helper
# =============================================================================

def test_detect_machine_type_dumper_truck():
    from scripts.combus_builder.generator import _detect_machine_type
    ctx = _ctx(["MACHINE_TYPE_DUMPER_TRUCK"])
    assert _detect_machine_type(ctx) == "MACHINE_TYPE_DUMPER_TRUCK"


def test_detect_machine_type_unconfigured():
    from scripts.combus_builder.generator import _detect_machine_type
    ctx = _ctx([])
    assert _detect_machine_type(ctx) == "UNCONFIGURED"


def test_detect_machine_type_ambiguous_surfaces():
    from scripts.combus_builder.generator import _detect_machine_type
    ctx = _ctx(["MACHINE_TYPE_DUMPER_TRUCK", "MACHINE_TYPE_EXCAVATOR"])
    assert _detect_machine_type(ctx).startswith("AMBIGUOUS:")


def test_detect_machine_type_with_value_works():
    from scripts.combus_builder.generator import _detect_machine_type
    ctx = BuildContext(
        buildroot=Path("/tmp"),
        defines=frozenset(),
        defines_with_value={"MACHINE_TYPE_DUMPER_TRUCK": "1"},
        cppdefines_source="override",
    )
    assert _detect_machine_type(ctx) == "MACHINE_TYPE_DUMPER_TRUCK"


# =============================================================================
# Determinism: same input → same hash across re-runs
# =============================================================================

def test_md5_deterministic_across_re_runs():
    chs = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE", direction=["uplink"]),
        _ch("L1", scope="LOCAL", direction=["both"]),
    ]})
    sel = select_views(chs)
    h1 = compute_view_hash(sel.local)
    h2 = compute_view_hash(sel.local)
    h3 = compute_view_hash(sel.local)
    assert h1.md5_hex == h2.md5_hex == h3.md5_hex


def test_md5_deterministic_across_file_discovery_order():
    """Same channels in different YAML file order → identical hash.

    This mirrors the A5 determinism property already validated on the
    view selection layer; A7 extends it to the hash layer.
    """
    chs1 = _canon_from_yamls({"channels": [
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
        _ch("L2", scope="LOCAL"),
    ]})
    chs2 = _canon_from_yamls({"channels": [
        _ch("L2", scope="LOCAL"),
        _ch("R1", scope="REMOTE"),
        _ch("L1", scope="LOCAL"),
    ]})
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    assert compute_view_hash(sel1.local).md5_hex == compute_view_hash(sel2.local).md5_hex
    assert compute_view_hash(sel1.remote).md5_hex == compute_view_hash(sel2.remote).md5_hex
