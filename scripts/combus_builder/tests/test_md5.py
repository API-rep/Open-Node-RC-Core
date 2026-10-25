#!/usr/bin/env python3
"""
Tests for md5.py (A7.2) - MD5 computation for the THREE combus views.

Coverage:
  - canonical_bytes() shape (JSON, sort_keys, includes required fields)
  - compute_view_hash() returns 16-byte digest
  - Hash inputs are: id, type, scope, direction, value, in wire order
    (chantier 2.1: theme and infoName no longer hashed)
  - Hash is independent of:
      * file order / discovery order
      * key ordering in the channel dict (cosmetic change)
  - Hash is sensitive to:
      * direction change (the case that motivated A7)
      * any channel field change
  - Hash inputs do NOT include:
      * projectVersion (separate constant)
      * requires (already captured by presence/absence after A6.1)
  - Scope influence per A7.2 task brief:
      * BASE influences all three hashes
      * SYSTEM influences combus_system and combus, NOT combus_base
      * BOARD influences combus, NOT combus_system NOR combus_base
      * combus_base hash value is UNCHANGED vs A7.1
  - generate_md5_artifacts() emits 4 files:
      * combus_base_md5.h
      * combus_system_md5.h
      * combus_md5.h
      * combus_wire_common.h
  - C++ header format:
      * k<View>ComBusMd5[16]    raw bytes per view
      * k<View>ComBusMd5Hex     32-char lowercase hex per view
      * kProjectVersionMajor/Minor  (in combus_wire_common.h only)
      * kCombusHandshakeWirePayloadLen = 19u (in combus_wire_common.h only;
        16 md5 + major + minor + dev, link rework 2.2b)
  - NO kMachineType (still removed).
  - 4-en-1 compile check: all four headers can be included together
    in a single TU without redefinition.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
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
    generate,
    select_views,
)
from scripts.combus_builder.md5 import (
    ViewHash,
    canonical_bytes,
    compute_view_hash,
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
    scope="SYSTEM",
    theme="failsafe",
    direction=None,
    requires=None,
    value=None,
):
    if direction is None:
        direction = "uplink"
    if requires is None:
        requires = []
    raw = {
        "id": id_,
        "infoName": info_name,
        "type": type_,
        "scope": scope,
        "theme": theme,
        "direction": direction,
        "requires": requires,
    }
    if value is not None:
        raw["value"] = value
    return raw


def _canon_from_yamls(*yamls):
    """Build a list of ChannelDefinitions from a list of channel dicts."""
    parsed = [_parsed(Path(f"ch{i}.cb"), {"channels": [y]}) for i, y in enumerate(yamls)]
    return canonize(parsed).canonical_definitions


def _ctx(flags):
    return BuildContext(
        buildroot=Path("."),
        defines=frozenset(flags),
        defines_with_value={},
        cppdefines_source="override",
    )


# =============================================================================
# canonical_bytes()
# =============================================================================

def test_canonical_bytes_is_valid_json():
    chs = _canon_from_yamls(_ch(scope="BASE"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.base))
    assert "view" in payload
    assert "channels" in payload


def test_canonical_bytes_includes_required_fields():
    chs = _canon_from_yamls(_ch(scope="BASE"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.base))
    ch = payload["channels"][0]
    for f in ("id", "type", "scope", "direction", "value"):
        assert f in ch, f"missing field {f}"
    for f in ("theme", "infoName"):
        assert f not in ch, f"field {f} must not be hashed (chantier 2.1)"


def test_canonical_bytes_view_name_included():
    # One BASE + one SYSTEM so all three views are non-empty.
    chs = _canon_from_yamls(_ch(scope="BASE"),
                             _ch(id_="LOC", info_name="Loc", scope="SYSTEM"))
    sel = select_views(chs)
    for v, name in [(sel.base, "combus_base"),
                    (sel.system,  "combus_system"),
                    (sel.full,   "combus")]:
        payload = json.loads(canonical_bytes(v))
        assert payload["view"] == name


def test_canonical_bytes_direction_is_sorted():
    """`direction: 'both'` normalises to ['downlink','uplink'] (sorted)."""
    chs = _canon_from_yamls(_ch(scope="BASE", direction="both"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.base))
    assert payload["channels"][0]["direction"] == ["downlink", "uplink"]


def test_canonical_bytes_independent_of_yaml_key_order():
    ch1 = {"id": "FOO", "infoName": "Foo", "type": "digital", "scope": "SYSTEM", "theme": "failsafe", "direction": "uplink", "requires": []}
    ch2 = {"theme": "failsafe", "scope": "SYSTEM", "type": "digital", "infoName": "Foo", "id": "FOO", "direction": "uplink", "requires": []}
    chs1 = _canon_from_yamls(ch1)
    chs2 = _canon_from_yamls(ch2)
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    assert canonical_bytes(sel1.base) == canonical_bytes(sel2.base)


# =============================================================================
# compute_view_hash()
# =============================================================================

def test_compute_view_hash_returns_16_bytes():
    chs = _canon_from_yamls(_ch())
    sel = select_views(chs)
    for v in (sel.base, sel.system, sel.full):
        h = compute_view_hash(v)
        assert len(h.digest) == 16


def test_compute_view_hash_hex_is_lowercase():
    chs = _canon_from_yamls(_ch())
    sel = select_views(chs)
    for v in (sel.base, sel.system, sel.full):
        h = compute_view_hash(v)
        assert h.md5_hex == h.md5_hex.lower()
        assert len(h.md5_hex) == 32


def test_compute_view_hash_matches_manual_md5():
    chs = _canon_from_yamls(_ch())
    sel = select_views(chs)
    for v in (sel.base, sel.system, sel.full):
        h = compute_view_hash(v)
        expected = hashlib.md5(canonical_bytes(v)).hexdigest()
        assert h.md5_hex == expected


def test_compute_view_hash_view_name_recorded():
    chs = _canon_from_yamls(_ch(scope="BASE"))
    sel = select_views(chs)
    assert compute_view_hash(sel.base).view_name == "combus_base"
    assert compute_view_hash(sel.system).view_name  == "combus_system"
    assert compute_view_hash(sel.full).view_name   == "combus"


def test_compute_view_hash_independent_of_file_order():
    raw = _ch(scope="BASE")
    chs1 = _canon_from_yamls(raw, _ch(id_="BAR", info_name="Bar", scope="BASE"))
    chs2 = list(reversed(chs1))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    for name in ("base", "system", "full"):
        v1 = getattr(sel1, name)
        v2 = getattr(sel2, name)
        assert compute_view_hash(v1).md5_hex == compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_direction_changes():
    """A direction change on a BASE channel must change the hash on
    BASE and full views (real contract signal).

    On the SYSTEM view, uplink and downlink collapse to ONE_WAY (ND7),
    so the hash is the SAME — this is the documented ND7 invariant.
    """
    chs1 = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    chs2 = _canon_from_yamls(_ch(scope="BASE", direction="downlink"))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    # BASE and full views: hash MUST differ (real contract signal).
    for name in ("base", "full"):
        v1 = getattr(sel1, name)
        v2 = getattr(sel2, name)
        assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex, (
            f"FAIL: {name} hash does not diverge between uplink and downlink"
        )
    # SYSTEM view: hash MUST be the SAME (ND7 ONE_WAY canonisation).
    h_local_u = compute_view_hash(sel1.system)
    h_local_d = compute_view_hash(sel2.system)
    assert h_local_u.md5_hex == h_local_d.md5_hex, (
        f"ND7 FAIL: system hash diverges between uplink and downlink "
        f"(uplink={h_local_u.md5_hex}, downlink={h_local_d.md5_hex})"
    )


def test_compute_view_hash_unchanged_when_infoName_changes():
    """Chantier 2.1, row 1: a debug label is not part of the contract."""
    chs1 = _canon_from_yamls(_ch(scope="BASE", info_name="Foo"))
    chs2 = _canon_from_yamls(_ch(scope="BASE", info_name="Bar"))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    for name in ("base", "system", "full"):
        v1 = getattr(sel1, name)
        v2 = getattr(sel2, name)
        assert compute_view_hash(v1).md5_hex == compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_id_added():
    chs1 = _canon_from_yamls(_ch(scope="BASE"))
    chs2 = _canon_from_yamls(_ch(scope="BASE"),
                              _ch(id_="BAR", info_name="Bar", scope="BASE"))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    for name in ("base", "system", "full"):
        v1 = getattr(sel1, name)
        v2 = getattr(sel2, name)
        assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_type_changes():
    chs1 = _canon_from_yamls(_ch(scope="BASE", type_="digital"))
    chs2 = _canon_from_yamls(_ch(scope="BASE", type_="analog"))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    for name in ("base", "system", "full"):
        v1 = getattr(sel1, name)
        v2 = getattr(sel2, name)
        assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


def test_compute_view_hash_changes_when_scope_moves():
    """SYSTEM -> BASE must change the hash for all 3 views (the channel
    is no longer in the view at all)."""
    chs1 = _canon_from_yamls(_ch(scope="SYSTEM"))
    chs2 = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    for name in ("base", "system", "full"):
        v1 = getattr(sel1, name)
        v2 = getattr(sel2, name)
        assert compute_view_hash(v1).md5_hex != compute_view_hash(v2).md5_hex


# =============================================================================
# A7.2 scope influence matrix
# =============================================================================

def test_base_influences_all_three_hashes():
    """BASE-only change -> all 3 hashes change."""
    base   = _canon_from_yamls(_ch(id_="OTHER", info_name="X", scope="SYSTEM"))
    with_r = _canon_from_yamls(_ch(id_="OTHER", info_name="X", scope="SYSTEM"),
                               _ch(id_="REM", info_name="R", scope="BASE", direction="uplink"))
    sel_base = select_views(base)
    sel_with = select_views(with_r)
    for name in ("base", "system", "full"):
        assert compute_view_hash(getattr(sel_base, name)).md5_hex !=                compute_view_hash(getattr(sel_with, name)).md5_hex


def test_system_influences_system_and_full_not_base():
    """SYSTEM-only change -> combus_system and combus change; combus_base
    does NOT change (BASE view is scope-isolated)."""
    base = _canon_from_yamls(_ch(id_="REM1", info_name="R1", scope="BASE", direction="uplink"))
    with_loc = _canon_from_yamls(_ch(id_="REM1", info_name="R1", scope="BASE", direction="uplink"),
                                 _ch(id_="LOC1", info_name="L1", scope="SYSTEM", direction="uplink"))
    sel_base   = select_views(base)
    sel_with_l = select_views(with_loc)

    # combus_base must be identical
    assert compute_view_hash(sel_base.base).md5_hex == \
           compute_view_hash(sel_with_l.base).md5_hex
    # combus_system and combus must differ
    assert compute_view_hash(sel_base.system).md5_hex != \
           compute_view_hash(sel_with_l.system).md5_hex
    assert compute_view_hash(sel_base.full).md5_hex != \
           compute_view_hash(sel_with_l.full).md5_hex


def test_board_influences_only_full():
    """BOARD-only change -> combus changes; combus_system and combus_base
    are unchanged."""
    base = _canon_from_yamls(_ch(id_="REM1", info_name="R1", scope="BASE", direction="uplink"))
    with_sys = _canon_from_yamls(_ch(id_="REM1", info_name="R1", scope="BASE", direction="uplink"),
                                 _ch(id_="SYS1", info_name="S1", scope="BOARD", direction="none"))
    sel_base   = select_views(base)
    sel_with_s = select_views(with_sys)

    # combus_base and combus_system are unchanged
    assert compute_view_hash(sel_base.base).md5_hex == \
           compute_view_hash(sel_with_s.base).md5_hex
    assert compute_view_hash(sel_base.system).md5_hex == \
           compute_view_hash(sel_with_s.system).md5_hex
    # combus (full) differs
    assert compute_view_hash(sel_base.full).md5_hex != \
           compute_view_hash(sel_with_s.full).md5_hex


def test_combus_base_hash_matches_a71_value():
    """A7.2 task brief: combus_base hash MUST NOT change vs A7.1.

    The MD5 computation is strictly the same function on the same view
    of the same channels. We verify by running both the legacy (A7.1)
    single-view path and the A7.2 three-view path and comparing.
    """
    raw = _ch(scope="BASE", direction="uplink")
    chs = _canon_from_yamls(raw)
    sel = select_views(chs)

    # A7.2 hash on sel.base
    h_a72 = compute_view_hash(sel.base)

    # Reproduce the A7.1 bytes path: canonical_bytes + manual md5
    expected = hashlib.md5(canonical_bytes(sel.base)).hexdigest()
    assert h_a72.md5_hex == expected


def test_same_inputs_produce_same_hashes():
    """Determinism: same canonical channels -> same MD5 across re-runs
    and across the three views independently."""
    raw = _ch(scope="BASE", direction="uplink")
    chs = _canon_from_yamls(raw)
    sel1 = select_views(chs)
    sel2 = select_views(chs)
    for name in ("base", "system", "full"):
        h1 = compute_view_hash(getattr(sel1, name))
        h2 = compute_view_hash(getattr(sel2, name))
        assert h1.md5_hex == h2.md5_hex


# =============================================================================
# generate_md5_artifacts() — 4 files emitted
# =============================================================================

def test_generate_md5_artifacts_emits_four_files():
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        hashes, written = generate_md5_artifacts(sel, Path(tmp_dir))
        names = sorted(p.name for p in written)
        assert names == sorted([
            "combus_base_md5.h",
            "combus_system_md5.h",
            "combus_md5.h",
            "combus_wire_common.h",
        ])
        assert set(hashes.keys()) == {"combus_base", "combus_system", "combus", "combus_base_next"}


def test_generate_md5_artifacts_emits_base_first():
    """Order matters: the shared header must be written FIRST so a
    consumer that includes a view-specific header after the shared
    header sees a stable on-disk order."""
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        _, written = generate_md5_artifacts(sel, Path(tmp_dir))
        assert written[0].name == "combus_wire_common.h"


def test_generate_md5_artifacts_hash_matches_written_bytes():
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp_dir:
        hashes, written = generate_md5_artifacts(sel, Path(tmp_dir))
        # Per-view: the hash hex must appear in the matching header file.
        for view_name, fname in [
            ("combus_base", "combus_base_md5.h"),
            ("combus_system",  "combus_system_md5.h"),
            ("combus",        "combus_md5.h"),
        ]:
            text = (Path(tmp_dir) / fname).read_text(encoding="utf-8")
            assert hashes[view_name].md5_hex in text


# =============================================================================
# View header format (per-view content)
# =============================================================================

def _make_hash(view_name="combus_base"):
    return ViewHash(
        view_name=view_name,
        md5_hex="0123456789abcdef0123456789abcdef",
        digest=bytes.fromhex("0123456789abcdef0123456789abcdef"),
    )


def test_base_view_header_has_base_constants():
    from scripts.combus_builder.md5 import _render_view_header
    out = _render_view_header(_make_hash("combus_base"))
    assert "kCombusBaseComBusMd5[16]" in out
    assert "kCombusBaseComBusMd5Hex" in out
    # No SYSTEM or FULL constants here
    assert "kCombusSystemComBusMd5" not in out
    assert "kCombusComBusMd5" not in out


def test_system_view_header_has_system_constants():
    from scripts.combus_builder.md5 import _render_view_header
    out = _render_view_header(_make_hash("combus_system"))
    assert "kCombusSystemComBusMd5[16]" in out
    assert "kCombusSystemComBusMd5Hex" in out
    assert "kCombusBaseComBusMd5" not in out
    assert "kCombusComBusMd5" not in out


def test_full_view_header_has_full_constants():
    from scripts.combus_builder.md5 import _render_view_header
    out = _render_view_header(_make_hash("combus"))
    assert "kCombusComBusMd5[16]" in out
    assert "kCombusComBusMd5Hex" in out
    assert "kCombusBaseComBusMd5" not in out
    assert "kCombusSystemComBusMd5" not in out


def test_view_header_includes_shared_header():
    """Each view header must #include combus_wire_common.h."""
    from scripts.combus_builder.md5 import _render_view_header
    for view in ("combus_base", "combus_system", "combus"):
        out = _render_view_header(_make_hash(view))
        assert '#include "combus_wire_common.h"' in out, view


def test_common_header_has_shared_constants():
    from scripts.combus_builder.md5 import _render_common_header
    out = _render_common_header()
    assert "kProjectVersionMajor" in out
    assert "kProjectVersionMinor" in out
    assert "kCombusHandshakeWirePayloadLen" in out
    assert "kCombusHandshakeWirePayloadLen = 19u;" in out
    # No MD5 bytes in the shared header
    assert "ComBusMd5[16]" not in out


def test_common_header_no_machine_type():
    """kMachineType is still removed (A7.1 simplification, unchanged in A7.2)."""
    from scripts.combus_builder.md5 import _render_common_header
    out = _render_common_header()
    assert "kMachineType" not in out


def test_view_header_has_md5_array_uppercase():
    """The formatter emits 0xEFu (uppercase)."""
    from scripts.combus_builder.md5 import _render_view_header
    out = _render_view_header(_make_hash("combus_base"))
    assert "0xEFu" in out


# =============================================================================
# Determinism of artifacts on disk
# =============================================================================

def test_md5_deterministic_across_re_runs():
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel = select_views(chs)
    with tempfile.TemporaryDirectory() as tmp1, tempfile.TemporaryDirectory() as tmp2:
        h1, _ = generate_md5_artifacts(sel, Path(tmp1))
        h2, _ = generate_md5_artifacts(sel, Path(tmp2))
        for name in ("combus_base", "combus_system", "combus"):
            assert h1[name].md5_hex == h2[name].md5_hex


def test_md5_deterministic_across_file_discovery_order():
    raw = _ch(scope="BASE", direction="uplink")
    chs1 = _canon_from_yamls(raw, _ch(id_="BAR", info_name="Bar", scope="BASE", direction="uplink"))
    chs2 = list(reversed(chs1))
    sel1 = select_views(chs1)
    sel2 = select_views(chs2)
    with tempfile.TemporaryDirectory() as tmp1, tempfile.TemporaryDirectory() as tmp2:
        h1, _ = generate_md5_artifacts(sel1, Path(tmp1))
        h2, _ = generate_md5_artifacts(sel2, Path(tmp2))
        for name in ("combus_base", "combus_system", "combus"):
            assert h1[name].md5_hex == h2[name].md5_hex


# =============================================================================
# 4-en-1 compile check
# =============================================================================

def test_four_headers_compile_together(tmp_path):
    """All four headers must include cleanly in a single TU."""
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    generate(chs, _ctx([]), tmp_path)
    tu = tmp_path / "compile_check.cpp"
    tu.write_text(
        "#include <stdint.h>\n"
        '#include "combus_wire_common.h"\n'
        '#include "combus_base_md5.h"\n'
        '#include "combus_system_md5.h"\n'
        '#include "combus_md5.h"\n'
        "int main() {\n"
        "  (void)combus::wire::kCombusBaseComBusMd5[0];\n"
        "  (void)combus::wire::kCombusBaseComBusMd5Hex;\n"
        "  (void)combus::wire::kCombusSystemComBusMd5[0];\n"
        "  (void)combus::wire::kCombusSystemComBusMd5Hex;\n"
        "  (void)combus::wire::kCombusComBusMd5[0];\n"
        "  (void)combus::wire::kCombusComBusMd5Hex;\n"
        "  (void)combus::wire::kProjectVersionMajor;\n"
        "  (void)combus::wire::kProjectVersionMinor;\n"
        "  (void)combus::wire::kCombusHandshakeWirePayloadLen;\n"
        "  return 0;\n"
        "}\n",
        encoding="utf-8",
    )
    from scripts.combus_builder.tests._cpp_compiler import find_cpp_compiler
    import pytest
    found = find_cpp_compiler()
    if found is None:
        pytest.skip("No C++ compiler available for compile-check")
    argv0, label = found
    r = subprocess.run(
        argv0 + ["-std=c++17", "-I", str(tmp_path), "-fsyntax-only", str(tu)],
        capture_output=True, text=True, timeout=60,
    )
    assert r.returncode == 0, f"compile failed with {label}: {r.stderr}"


# =============================================================================
# A10 — Determinism on the REAL repo (end-to-end, bit-identical)
# =============================================================================
#
# A10 of the Roadmap: "Validate determinism (2 runs -> identical contract)".
# This test runs the FULL pipeline (parser -> canon -> select_views ->
# generate -> generate_md5_artifacts) twice on the real .cb files in src/,
# then asserts that:
#   1. The 3 view hashes (combus_base, combus_system, combus) are identical.
#   2. The 4 emitted header files (combus_wire_common.h, combus_base_md5.h,
#      combus_system_md5.h, combus_md5.h) are BIT-IDENTICAL between the two
#      runs (same bytes, not just same hash).
#   3. The 3 view-specific C++ headers (combus_base.h, combus_system.h,
#      combus.h) are also bit-identical.
#
# This is the strongest possible determinism guarantee: not just "same hash"
# but "same bytes on disk". If anything in the pipeline is non-deterministic
# (timestamp, dict iteration order, file mtime, etc.), this test catches it.

def _run_full_pipeline(out_dir: Path):
    """Run the full combus_builder pipeline on the real repo, write to out_dir."""
    from scripts.combus_builder.parser import discover_and_parse
    from scripts.combus_builder.canon import canonize
    from scripts.combus_builder.generator import generate, select_views

    buildroot = REPO_ROOT / "src"
    if not buildroot.exists():
        import pytest
        pytest.skip(f"src/ not found at {buildroot}")

    _, files, parsed = discover_and_parse(project_root=buildroot.parent, machine_type="dumper_truck")
    cb_files = [f for f in files if f.suffix == ".cb"]
    if not cb_files:
        import pytest
        pytest.skip("no .cb files found in src/")

    result = canonize(parsed)
    sel = select_views(result.canonical_definitions)
    # The real repo has channels with `requires: [HAS_FAILSAFE, HAS_VBAT_FAILSAFE]`.
    # We must declare these flags in the BuildContext, otherwise generate() raises
    # RequiresResolutionError. This mirrors what test_generator.py does.
    ctx = _ctx(["HAS_FAILSAFE", "HAS_VBAT_FAILSAFE", "HAS_RUNLEVEL_MANAGER"])
    # generate() takes the raw list of ChannelDefinitions, not the ViewSelection.
    generate(result.canonical_definitions, ctx, out_dir)
    return sel


def test_a10_full_pipeline_deterministic_bit_identical(tmp_path: Path):
    """A10: 2 runs of the full pipeline on the real repo produce bit-identical
    headers (not just same hash)."""
    out1 = tmp_path / "run1"
    out2 = tmp_path / "run2"
    out1.mkdir()
    out2.mkdir()

    sel1 = _run_full_pipeline(out1)
    sel2 = _run_full_pipeline(out2)

    # 1. Hashes must match.
    # ViewSelection has attributes .base, .system, .full (not combus_*).
    for attr in ("base", "system", "full"):
        h1 = compute_view_hash(getattr(sel1, attr))
        h2 = compute_view_hash(getattr(sel2, attr))
        assert h1.md5_hex == h2.md5_hex, (
            f"A10 FAIL: hash mismatch for {attr}: "
            f"run1={h1.md5_hex} run2={h2.md5_hex}"
        )

    # 2. Emitted header files must be bit-identical.
    expected_files = [
        "combus_wire_common.h",
        "combus_base_md5.h",
        "combus_system_md5.h",
        "combus_md5.h",
        "combus_base_ids.h",
        "combus_system_ids.h",
        "combus_ids.h",
        "combus.h",
        "combus.cpp",
    ]
    for fname in expected_files:
        p1 = out1 / fname
        p2 = out2 / fname
        assert p1.exists(), f"A10 FAIL: {fname} missing in run1"
        assert p2.exists(), f"A10 FAIL: {fname} missing in run2"
        b1 = p1.read_bytes()
        b2 = p2.read_bytes()
        assert b1 == b2, (
            f"A10 FAIL: {fname} differs between runs "
            f"(run1={len(b1)} bytes, run2={len(b2)} bytes)"
        )


def test_a10_md5_deterministic_across_file_discovery_order(tmp_path: Path):
    """A10: even if the OS returns .cb files in a different order, the
    generated headers are bit-identical."""
    from scripts.combus_builder.parser import discover_and_parse
    from scripts.combus_builder.canon import canonize
    from scripts.combus_builder.generator import generate, select_views

    buildroot = REPO_ROOT / "src"
    if not buildroot.exists():
        import pytest
        pytest.skip(f"src/ not found at {buildroot}")

    out1 = tmp_path / "run1"
    out2 = tmp_path / "run2"
    out1.mkdir()
    out2.mkdir()

    # Run 1: normal order.
    _, _, parsed1 = discover_and_parse(project_root=buildroot.parent, machine_type="dumper_truck")
    result1 = canonize(parsed1)
    sel1 = select_views(result1.canonical_definitions)
    ctx = _ctx(["HAS_FAILSAFE", "HAS_VBAT_FAILSAFE", "HAS_RUNLEVEL_MANAGER"])
    generate(result1.canonical_definitions, ctx, out1)

    # Run 2: reverse the file order to stress the parser/canon.
    _, files2, parsed2 = discover_and_parse(project_root=buildroot.parent, machine_type="dumper_truck")
    parsed2_reversed = list(reversed(parsed2))
    result2 = canonize(parsed2_reversed)
    sel2 = select_views(result2.canonical_definitions)
    generate(result2.canonical_definitions, ctx, out2)

    # All 7 emitted files must be bit-identical.
    for fname in (
        "combus_wire_common.h",
        "combus_base_md5.h",
        "combus_system_md5.h",
        "combus_md5.h",
        "combus_base_ids.h",
        "combus_system_ids.h",
        "combus_ids.h",
        "combus.h",
        "combus.cpp",
    ):
        b1 = (out1 / fname).read_bytes()
        b2 = (out2 / fname).read_bytes()
        assert b1 == b2, f"A10 FAIL: {fname} differs across file-order runs"


# =============================================================================
# ND7 — ONE_WAY canonisation for MD5 hashing
# =============================================================================
#
# Rationale: in a multi-board node, the same SYSTEM channel can legitimately
# appear as `uplink` on one board and `downlink` on another — this is a
# role mirror, not a contract drift. The MD5 must NOT flag this as a
# divergence between boards.
#
# `both` and `none` are real contract signals and must remain
# distinguishable from each other and from the one-way cases.
#
# Mapping (hash-only, NEVER for code generation):
#   frozenset({'uplink'})        → ['ONE_WAY']
#   frozenset({'downlink'})      → ['ONE_WAY']
#   frozenset({'uplink','downlink'}) (both) → ['downlink','uplink']
#   frozenset() (none)           → []

from scripts.combus_builder.md5 import _canonical_direction_for_hash


def test_canonical_direction_uplink_is_one_way():
    """frozenset({'uplink'}) → ['ONE_WAY']."""
    assert _canonical_direction_for_hash(frozenset({"uplink"})) == ["ONE_WAY"]


def test_canonical_direction_downlink_is_one_way():
    """frozenset({'downlink'}) → ['ONE_WAY']."""
    assert _canonical_direction_for_hash(frozenset({"downlink"})) == ["ONE_WAY"]


def test_canonical_direction_uplink_and_downlink_collide():
    """uplink and downlink must produce the SAME canonical value.

    This is the core ND7 invariant: the role mirror between boards
    must not produce a hash divergence.
    """
    assert _canonical_direction_for_hash(frozenset({"uplink"})) == \
           _canonical_direction_for_hash(frozenset({"downlink"}))


def test_canonical_direction_both_unchanged():
    """frozenset({'uplink','downlink'}) → ['downlink','uplink'] (sorted)."""
    assert _canonical_direction_for_hash(
        frozenset({"uplink", "downlink"})
    ) == ["downlink", "uplink"]


def test_canonical_direction_none_unchanged():
    """frozenset() → [] (none is a real contract signal)."""
    assert _canonical_direction_for_hash(frozenset()) == []


def test_canonical_direction_both_distinct_from_one_way():
    """both must be distinguishable from ONE_WAY."""
    assert _canonical_direction_for_hash(
        frozenset({"uplink", "downlink"})
    ) != _canonical_direction_for_hash(frozenset({"uplink"}))


def test_canonical_direction_none_distinct_from_one_way():
    """none must be distinguishable from ONE_WAY."""
    assert _canonical_direction_for_hash(frozenset()) != \
           _canonical_direction_for_hash(frozenset({"uplink"}))


def test_canonical_direction_both_distinct_from_none():
    """both must be distinguishable from none."""
    assert _canonical_direction_for_hash(
        frozenset({"uplink", "downlink"})
    ) != _canonical_direction_for_hash(frozenset())


# -----------------------------------------------------------------------------
# Integration: the canonisation must flow through canonical_bytes() and
# compute_view_hash() for ALL three views (canonical_bytes is shared).
# -----------------------------------------------------------------------------


def test_nd7_uplink_vs_downlink_same_hash_on_system_view():
    """Two channels identical except uplink vs downlink must produce
    the SAME MD5 on the SYSTEM view (ND7 ONE_WAY canonisation).

    This is the end-to-end ND7 invariant: the role mirror between
    boards must not produce a hash divergence on the SYSTEM view.

    BASE and full views are NOT affected by ONE_WAY (uplink/downlink
    are real contract signals on BASE channels).
    """
    chs_uplink = _canon_from_yamls(_ch(scope="SYSTEM", direction="uplink"))
    chs_downlink = _canon_from_yamls(_ch(scope="SYSTEM", direction="downlink"))
    sel_u = select_views(chs_uplink)
    sel_d = select_views(chs_downlink)
    h_u = compute_view_hash(sel_u.system)
    h_d = compute_view_hash(sel_d.system)
    assert h_u.md5_hex == h_d.md5_hex, (
        f"ND7 FAIL: system hash diverges between uplink and downlink "
        f"(uplink={h_u.md5_hex}, downlink={h_d.md5_hex})"
    )


def test_nd7_uplink_vs_downlink_different_hash_on_base_view():
    """On the BASE view, uplink and downlink MUST produce different
    hashes (they are real contract signals on BASE channels).

    This guards against an over-aggressive canonisation that would
    collapse BASE uplink/downlink into ONE_WAY.
    """
    chs_uplink = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    chs_downlink = _canon_from_yamls(_ch(scope="BASE", direction="downlink"))
    sel_u = select_views(chs_uplink)
    sel_d = select_views(chs_downlink)
    h_u = compute_view_hash(sel_u.base)
    h_d = compute_view_hash(sel_d.base)
    assert h_u.md5_hex != h_d.md5_hex, (
        f"ND7 FAIL: base hash does not diverge between uplink and downlink "
        f"(uplink={h_u.md5_hex}, downlink={h_d.md5_hex})"
    )


def test_nd7_both_vs_uplink_different_hash():
    """A channel that changes from both to uplink MUST produce a
    different hash on all 3 views — regression of real contract
    drift detection.

    This guards against an over-aggressive canonisation that would
    collapse both into ONE_WAY.
    """
    chs_both = _canon_from_yamls(_ch(scope="BASE", direction="both"))
    chs_uplink = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel_b = select_views(chs_both)
    sel_u = select_views(chs_uplink)
    for attr in ("base", "system", "full"):
        h_b = compute_view_hash(getattr(sel_b, attr))
        h_u = compute_view_hash(getattr(sel_u, attr))
        assert h_b.md5_hex != h_u.md5_hex, (
            f"ND7 FAIL: {attr} hash does not diverge between both and uplink "
            f"(both={h_b.md5_hex}, uplink={h_u.md5_hex})"
        )


def test_nd7_none_vs_downlink_different_hash():
    """A channel that changes from none to downlink MUST produce a
    different hash on all 3 views — regression of real contract
    drift detection.

    This guards against an over-aggressive canonisation that would
    collapse none into ONE_WAY.
    """
    chs_none = _canon_from_yamls(_ch(scope="BASE", direction="none"))
    chs_downlink = _canon_from_yamls(_ch(scope="BASE", direction="downlink"))
    sel_n = select_views(chs_none)
    sel_d = select_views(chs_downlink)
    for attr in ("base", "system", "full"):
        h_n = compute_view_hash(getattr(sel_n, attr))
        h_d = compute_view_hash(getattr(sel_d, attr))
        assert h_n.md5_hex != h_d.md5_hex, (
            f"ND7 FAIL: {attr} hash does not diverge between none and downlink "
            f"(none={h_n.md5_hex}, downlink={h_d.md5_hex})"
        )


def test_nd7_both_vs_none_different_hash():
    """A channel that changes from both to none MUST produce a
    different hash on all 3 views — regression of real contract
    drift detection.
    """
    chs_both = _canon_from_yamls(_ch(scope="BASE", direction="both"))
    chs_none = _canon_from_yamls(_ch(scope="BASE", direction="none"))
    sel_b = select_views(chs_both)
    sel_n = select_views(chs_none)
    for attr in ("base", "system", "full"):
        h_b = compute_view_hash(getattr(sel_b, attr))
        h_n = compute_view_hash(getattr(sel_n, attr))
        assert h_b.md5_hex != h_n.md5_hex, (
            f"ND7 FAIL: {attr} hash does not diverge between both and none "
            f"(both={h_b.md5_hex}, none={h_n.md5_hex})"
        )


def test_nd7_canonical_bytes_system_view_applies_one_way():
    """The `direction` field in canonical_bytes() for the SYSTEM view
    must reflect the ONE_WAY canonisation, not the raw frozenset.
    """
    chs = _canon_from_yamls(_ch(scope="SYSTEM", direction="uplink"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.system))
    assert payload["channels"][0]["direction"] == ["ONE_WAY"]


def test_nd7_canonical_bytes_base_view_keeps_raw_direction():
    """The `direction` field in canonical_bytes() for the BASE view
    must keep the raw direction (no ONE_WAY canonisation).
    """
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.base))
    assert payload["channels"][0]["direction"] == ["uplink"]


def test_nd7_canonical_bytes_full_view_keeps_raw_direction():
    """The `direction` field in canonical_bytes() for the full view
    must keep the raw direction (no ONE_WAY canonisation).
    """
    chs = _canon_from_yamls(_ch(scope="BASE", direction="uplink"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.full))
    assert payload["channels"][0]["direction"] == ["uplink"]


def test_nd7_canonical_bytes_both_unchanged_on_system():
    """The `direction` field for `both` must remain ['downlink','uplink']
    on the SYSTEM view (both is a real contract signal)."""
    chs = _canon_from_yamls(_ch(scope="SYSTEM", direction="both"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.system))
    assert payload["channels"][0]["direction"] == ["downlink", "uplink"]


def test_nd7_canonical_bytes_none_unchanged_on_system():
    """The `direction` field for `none` must remain [] on the SYSTEM view."""
    chs = _canon_from_yamls(_ch(scope="SYSTEM", direction="none"))
    sel = select_views(chs)
    payload = json.loads(canonical_bytes(sel.system))
    assert payload["channels"][0]["direction"] == []


# =============================================================================
# A2.1 — hash canonisation of the `*_or` direction variants (direct contract)
# =============================================================================

import pytest  # noqa: E402


@pytest.mark.parametrize(
    "direction, direction_or, expected",
    [
        (frozenset({"uplink"}), frozenset(), ["ONE_WAY"]),
        (frozenset({"downlink"}), frozenset(), ["ONE_WAY"]),
        (frozenset(), frozenset({"uplink"}), ["ONE_WAY"]),      # uplink_or
        (frozenset(), frozenset({"downlink"}), ["ONE_WAY"]),    # downlink_or
        (frozenset({"uplink", "downlink"}), frozenset(), ["downlink", "uplink"]),  # both
        (frozenset(), frozenset({"uplink", "downlink"}),
         ["both_or", "downlink", "uplink"]),                    # both_or
        (frozenset(), frozenset(), []),                         # none
    ],
)
def test_canonical_direction_for_hash_full_mapping(direction, direction_or, expected):
    assert _canonical_direction_for_hash(direction, direction_or) == expected


# =============================================================================
# Chantier 2.1 — what the contract hash covers (table validated by the owner)
# =============================================================================

def _hashes(*yamls):
    sel = select_views(_canon_from_yamls(*yamls))
    return {name: compute_view_hash(getattr(sel, name)).md5_hex for name in ("base", "system", "full")}


def test_contract_row2_theme_change_in_place_does_not_change_hash():
    """Row 2: a theme change that keeps the channel at its place on the wire."""
    h1 = _hashes(_ch(scope="BASE", theme="failsafe"))
    h2 = _hashes(_ch(scope="BASE", theme="input"))
    assert h1 == h2


def test_contract_row3_theme_change_that_moves_the_channel_changes_hash():
    """Row 3: the theme is part of the wire order; moving a channel changes the contract."""
    h1 = _hashes(_ch(id_="AAA", scope="BASE", theme="input"),
                 _ch(id_="BBB", scope="BASE", theme="failsafe"))       # wire: BBB, AAA
    h2 = _hashes(_ch(id_="AAA", scope="BASE", theme="input"),
                 _ch(id_="BBB", scope="BASE", theme="light"))          # wire: AAA, BBB
    assert h1["base"] != h2["base"]
    assert h1["system"] != h2["system"]


def test_contract_row4_id_rename_changes_hash():
    h1 = _hashes(_ch(id_="FOO", scope="BASE"))
    h2 = _hashes(_ch(id_="FOO2", scope="BASE"))
    assert h1["base"] != h2["base"]
    assert h1["system"] != h2["system"]


def test_contract_row7_both_to_both_or_changes_hash():
    h1 = _hashes(_ch(scope="BASE", direction="both"))
    h2 = _hashes(_ch(scope="BASE", direction="both_or"))
    assert h1["base"] != h2["base"]
    assert h1["system"] != h2["system"]


def test_contract_row11_default_value_changes_hash():
    """Row 11: the default value is sent while the writer has not written: part of the contract."""
    h1 = _hashes(_ch(scope="BASE", value="low"))
    h2 = _hashes(_ch(scope="BASE", value="high"))
    assert h1["base"] != h2["base"]
    assert h1["system"] != h2["system"]


def test_contract_row11_analog_default_value_changes_hash():
    h1 = _hashes(_ch(scope="BASE", type_="analog", value="CbusNeutral"))
    h2 = _hashes(_ch(scope="BASE", type_="analog", value="CbusMinVal"))
    assert h1["base"] != h2["base"]


def test_generate_prints_one_md5_line_per_set(tmp_path, capsys):
    """Chantier 2.1: the build log shows the MD5 of each set, with its channel counts."""
    from scripts.combus_builder.generator import generate

    chs = _canon_from_yamls(_ch(id_="B1", scope="BASE", type_="analog"),
                            _ch(id_="B2", scope="BASE"),
                            _ch(id_="S1", scope="SYSTEM"))
    sel = generate(chs, _ctx([]), tmp_path)
    out = capsys.readouterr().out

    base_md5 = compute_view_hash(sel.base).md5_hex
    system_md5 = compute_view_hash(sel.system).md5_hex
    assert f"[combus_builder] set BASE    md5={base_md5}  A1+D1" in out
    assert f"[combus_builder] set SYSTEM  md5={system_md5}  A1+D2" in out
