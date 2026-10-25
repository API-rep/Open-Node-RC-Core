#!/usr/bin/env python3
"""
Tests for the ND8 (correctif 1/3) machine-type discovery filter.

Coverage:
  - machine_type.detect_machine_type():
      * No MACHINE_TYPE_* flag -> None
      * Exactly one MACHINE_TYPE_* flag -> the corresponding dir segment
      * Two MACHINE_TYPE_* flags -> AmbiguousMachineTypeError
      * Valued defines (e.g. MACHINE_TYPE_X=1) are also detected
  - machine_type.filter_files_by_machine_type():
      * None (no machine type) -> keep the generic files, exclude every
        machine type profile (link rework 3.1: peer types share names)
      * "dumper_truck" -> keep dumper_truck/, exclude excavator/ and loader/
      * Generic .cb files (outside core/config/machines/) always kept
      * Order of input is preserved
  - parser.discover_config_files() integration:
      * The filter is applied after the file walk, before return
      * The buildroot is unchanged when machine_type is None
  - parser.discover_and_parse() integration:
      * The `machine_type` parameter is forwarded to discover_config_files()
      * Existing call sites that do NOT pass machine_type still work
        (backward compat with test_nd1_*, test_md5, etc.)
  - End-to-end pipeline test:
      * A SYSTEM channel declared under machines/dumper_truck/ with
        requires=[IS_MAINBOARD] on mainboard only diffuses to the sister
        board as downlink after the filter is applied (the filter
        removes any excavator/ .cb, leaving dumper_truck/ .cb intact).

Run with:
    python scripts/combus_builder/tests/test_machine_type_filter.py
"""
from __future__ import annotations

import io
import sys
import tempfile
import textwrap
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import pytest

from scripts.combus_builder.diffusion import diffuse_system_channels
from scripts.combus_builder.machine_type import (
    AmbiguousMachineTypeError,
    MACHINE_TYPE_FLAGS,
    _flag_to_dir_segment,
    all_machine_type_dirs,
    detect_machine_type,
    filter_files_by_machine_type,
    is_machine_type_path,
)
from scripts.combus_builder.flags import BuildContext
from scripts.combus_builder.node_context import NodeContext
from scripts.combus_builder.parser import (
    discover_and_parse,
    discover_config_files,
)


# =============================================================================
# Helpers
# =============================================================================

def _ctx_with(defines: set[str], values: dict[str, str] | None = None,
              src: Path | None = None) -> BuildContext:
    """Build a BuildContext for tests."""
    return BuildContext(
        buildroot=(src or Path("/tmp")).resolve(),
        defines=frozenset(defines),
        defines_with_value=dict(values or {}),
        cppdefines_source="test",
    )


def _write_cb(path: Path, body: str) -> Path:
    """Write a minimal valid .cb YAML body to `path` and return the path."""
    path.write_text(textwrap.dedent(body), encoding="utf-8")
    return path


# =============================================================================
# 1. detect_machine_type()
# =============================================================================

def test_detect_returns_none_when_no_machine_type_flag():
    """A context with zero MACHINE_TYPE_* flags resolves to None."""
    ctx = _ctx_with({"IS_MACHINE", "MACHINE_VOLVO_A60_H_BRUDER"})
    assert detect_machine_type(ctx) is None


def test_detect_dumper_truck_value_less():
    """MACHINE_TYPE_DUMPER_TRUCK (value-less) -> "dumper_truck"."""
    ctx = _ctx_with({"MACHINE_TYPE_DUMPER_TRUCK"})
    assert detect_machine_type(ctx) == "dumper_truck"


def test_detect_dumper_truck_valued():
    """MACHINE_TYPE_DUMPER_TRUCK=1 (valued) -> "dumper_truck".

    The detector must accept both the value-less and the valued form
    (BuildContext.has() abstracts over both, but this test pins the
    contract: a define declared as `-D MACHINE_TYPE_DUMPER_TRUCK=1`
    must still be recognised as a machine type selector).
    """
    ctx = _ctx_with(set(), values={"MACHINE_TYPE_DUMPER_TRUCK": "1"})
    assert detect_machine_type(ctx) == "dumper_truck"


def test_detect_excavator():
    """MACHINE_TYPE_EXCAVATOR -> "excavator"."""
    ctx = _ctx_with({"MACHINE_TYPE_EXCAVATOR"})
    assert detect_machine_type(ctx) == "excavator"


def test_detect_loader():
    """MACHINE_TYPE_LOADER -> "loader"."""
    ctx = _ctx_with({"MACHINE_TYPE_LOADER"})
    assert detect_machine_type(ctx) == "loader"


def test_detect_two_machine_type_flags_raises():
    """Two MACHINE_TYPE_* flags in the same ctx -> AmbiguousMachineTypeError.

    The detector must NOT silently pick a winner. A build that defines
    two machine types is a user error (typically a copy-paste in
    platformio.ini) and must be flagged loudly.
    """
    ctx = _ctx_with({"MACHINE_TYPE_DUMPER_TRUCK", "MACHINE_TYPE_EXCAVATOR"})
    try:
        detect_machine_type(ctx)
    except AmbiguousMachineTypeError as e:
        # Both flags must be reported (sorted, for determinism).
        assert e.flags == ["MACHINE_TYPE_DUMPER_TRUCK", "MACHINE_TYPE_EXCAVATOR"]
        return
    raise AssertionError("expected AmbiguousMachineTypeError")


def test_detect_with_other_unrelated_flags():
    """Unrelated defines do NOT confuse the detector."""
    ctx = _ctx_with({
        "IS_MACHINE",
        "MACHINE_VOLVO_A60_H_BRUDER",
        "HAS_FAILSAFE",
        "MACHINE_TYPE_DUMPER_TRUCK",
        "DEBUG_DASHBOARD",
    })
    assert detect_machine_type(ctx) == "dumper_truck"


def test_all_machine_type_dirs_lists_known_types():
    """all_machine_type_dirs() returns a directory segment per known flag."""
    dirs = all_machine_type_dirs()
    assert "dumper_truck" in dirs
    assert "excavator" in dirs
    assert "loader" in dirs
    # Sanity: same number of entries as known flags.
    assert len(dirs) == len(MACHINE_TYPE_FLAGS)


def test_flag_to_dir_segment_lowercase():
    """The flag-to-dir mapping is the lowercase form of the suffix."""
    assert _flag_to_dir_segment("MACHINE_TYPE_DUMPER_TRUCK") == "dumper_truck"
    assert _flag_to_dir_segment("MACHINE_TYPE_EXCAVATOR") == "excavator"


def test_flag_to_dir_segment_rejects_non_machine_type_flag():
    """A flag without the MACHINE_TYPE_ prefix raises ValueError."""
    try:
        _flag_to_dir_segment("IS_MACHINE")
    except ValueError:
        return
    raise AssertionError("expected ValueError for non-MACHINE_TYPE_ flag")


# =============================================================================
# 2. is_machine_type_path()
# =============================================================================

def _parts(*segments: str) -> Path:
    """Build a Path from a list of segments (Windows-safe: uses forward slashes)."""
    return Path(*segments)


def test_is_machine_type_path_under_active_dir():
    p = _parts("src", "core", "config", "machines", "dumper_truck",
               "combus", "steering_bus.cb")
    assert is_machine_type_path(p, ["dumper_truck", "excavator", "loader"])


def test_is_machine_type_path_under_other_dir():
    p = _parts("src", "core", "config", "machines", "excavator",
               "combus", "boom_bus.cb")
    assert is_machine_type_path(p, ["excavator", "loader"])


def test_is_machine_type_path_generic():
    p = _parts("src", "core", "system", "failsafe", "failsafe.cb")
    assert not is_machine_type_path(p, ["dumper_truck", "excavator", "loader"])


def test_is_machine_type_path_requires_full_marker():
    """A path that contains 'dumper_truck' but NOT after (core,config,machines) -> False.

    This guards against false positives on file names that happen to
    contain a machine-type segment.
    """
    p = _parts("src", "dumper_truck", "config", "machines", "dumper_truck.cb")
    # The marker (core, config, machines, X) is NOT contiguous, so the
    # function must return False.
    assert not is_machine_type_path(p, ["dumper_truck", "excavator", "loader"])


def test_is_machine_type_path_case_sensitive():
    """Segment case must match the canonical lowercase (per the codebase)."""
    p = _parts("src", "core", "config", "machines", "Dumper_Truck", "f.cb")
    # The directory is 'Dumper_Truck' (capital D/T), not 'dumper_truck' ->
    # not recognised.
    assert not is_machine_type_path(p, ["dumper_truck", "excavator", "loader"])


# =============================================================================
# 3. filter_files_by_machine_type()
# =============================================================================

def test_filter_none_keeps_only_generic_files():
    """machine_type=None -> no machine type profile (link rework 3.1: peer types share names)."""
    files = [
        _parts("src", "core", "config", "machines", "dumper_truck", "a.cb"),
        _parts("src", "core", "config", "machines", "excavator", "b.cb"),
        _parts("src", "core", "system", "failsafe", "failsafe.cb"),
    ]
    out = filter_files_by_machine_type(files, None)
    assert out == [files[2]]


def test_filter_dumper_truck_excludes_excavator():
    """machine_type='dumper_truck' excludes excavator/ and loader/, keeps dumper_truck/.

    The test uses representative .cb paths: one under each machine type
    and one generic (failsafe.cb). The generic path must always be
    kept, regardless of the active type.
    """
    files = [
        _parts("src", "core", "config", "machines", "dumper_truck", "a.cb"),
        _parts("src", "core", "config", "machines", "excavator", "b.cb"),
        _parts("src", "core", "config", "machines", "loader", "c.cb"),
        _parts("src", "core", "system", "failsafe", "failsafe.cb"),
    ]
    out = filter_files_by_machine_type(files, "dumper_truck")
    # dumper_truck kept, excavator + loader excluded, generic kept.
    assert len(out) == 2
    assert any("dumper_truck" in p.parts for p in out)
    assert any("failsafe" in p.parts for p in out)
    assert not any("excavator" in p.parts for p in out)
    assert not any("loader" in p.parts for p in out)


def test_filter_preserves_input_order():
    """The filter must preserve the order of the input list."""
    files = [
        _parts("src", "core", "system", "a.cb"),
        _parts("src", "core", "config", "machines", "excavator", "b.cb"),
        _parts("src", "core", "system", "c.cb"),
        _parts("src", "core", "config", "machines", "loader", "d.cb"),
    ]
    out = filter_files_by_machine_type(files, "dumper_truck")
    # Only the generic files survive; order preserved.
    assert [p.parts[-2] for p in out] == ["system", "system"]


def test_filter_active_dir_files_always_kept():
    """Files under the ACTIVE machine-type dir are always kept."""
    files = [
        _parts("src", "core", "config", "machines", "dumper_truck", "a.cb"),
        _parts("src", "core", "config", "machines", "dumper_truck",
               "combus", "b.cb"),
        _parts("src", "core", "config", "machines", "dumper_truck",
               "inputs", "input.cb"),
    ]
    out = filter_files_by_machine_type(files, "dumper_truck")
    assert len(out) == 3


# =============================================================================
# 4. parser.discover_config_files() integration
# =============================================================================

def test_discover_config_files_with_machine_type_excludes_others(tmp_path):
    """End-to-end: discover_config_files with a real filesystem and a
    machine_type filter excludes the wrong-type .cb files."""
    src = tmp_path / "src"
    src.mkdir()
    (src / "core" / "config" / "machines" / "dumper_truck" / "combus").mkdir(parents=True)
    (src / "core" / "config" / "machines" / "excavator" / "combus").mkdir(parents=True)
    (src / "core" / "config" / "machines" / "loader" / "combus").mkdir(parents=True)
    (src / "core" / "system" / "failsafe").mkdir(parents=True)
    _write_cb(src / "core" / "config" / "machines" / "dumper_truck" / "combus" / "steering.cb",
              "channels: []\n")
    _write_cb(src / "core" / "config" / "machines" / "excavator" / "combus" / "boom.cb",
              "channels: []\n")
    _write_cb(src / "core" / "config" / "machines" / "loader" / "combus" / "bucket.cb",
              "channels: []\n")
    _write_cb(src / "core" / "system" / "failsafe" / "failsafe.cb",
              "channels: []\n")

    # machine_type='dumper_truck' -> only dumper_truck/ + generic.
    found = discover_config_files(src, machine_type="dumper_truck")
    found_strs = [str(p) for p in found]
    assert any("dumper_truck" in s and "steering" in s for s in found_strs)
    assert any("failsafe" in s for s in found_strs)
    assert not any("excavator" in s for s in found_strs)
    assert not any("loader" in s for s in found_strs)


def test_discover_config_files_no_machine_type_keeps_no_type_profile(tmp_path):
    """No machine_type -> no machine type profile (link rework 3.1)."""
    src = tmp_path / "src"
    (src / "core" / "config" / "machines" / "dumper_truck").mkdir(parents=True)
    (src / "core" / "config" / "machines" / "excavator").mkdir(parents=True)
    _write_cb(src / "core" / "config" / "machines" / "dumper_truck" / "a.cb",
              "channels: []\n")
    _write_cb(src / "core" / "config" / "machines" / "excavator" / "b.cb",
              "channels: []\n")

    found = discover_config_files(src)  # default: no machine_type
    assert found == []


def test_discover_and_parse_forwards_machine_type(tmp_path):
    """discover_and_parse() forwards the machine_type to discover_config_files."""
    src = tmp_path / "src"
    (src / "core" / "config" / "machines" / "dumper_truck").mkdir(parents=True)
    (src / "core" / "config" / "machines" / "excavator").mkdir(parents=True)
    _write_cb(src / "core" / "config" / "machines" / "dumper_truck" / "a.cb",
              "channels: []\n")
    _write_cb(src / "core" / "config" / "machines" / "excavator" / "b.cb",
              "channels: []\n")

    # With machine_type='dumper_truck', the excavator/ .cb must NOT
    # appear in the returned parsed list.
    _buildroot, _files, parsed = discover_and_parse(
        project_root=tmp_path, machine_type="dumper_truck",
    )
    parsed_paths = [p for (p, _t, _d) in parsed]
    assert any("dumper_truck" in str(p) for p in parsed_paths)
    assert not any("excavator" in str(p) for p in parsed_paths)


def test_discover_and_parse_backward_compatible_no_machine_type(tmp_path):
    """Existing call sites that don't pass machine_type still work (legacy)."""
    src = tmp_path / "src"
    (src / "core" / "config" / "machines" / "dumper_truck").mkdir(parents=True)
    (src / "core" / "config" / "machines" / "excavator").mkdir(parents=True)
    _write_cb(src / "core" / "config" / "machines" / "dumper_truck" / "a.cb",
              "channels: []\n")
    _write_cb(src / "core" / "config" / "machines" / "excavator" / "b.cb",
              "channels: []\n")

    # No machine_type -> no machine type profile (link rework 3.1).
    _buildroot, _files, parsed = discover_and_parse(project_root=tmp_path)
    assert len(parsed) == 0


# =============================================================================
# 5. End-to-end pipeline test: ND2 diffusion unchanged by the filter
# =============================================================================

def test_filter_does_not_change_nd2_diffusion_behaviour():
    """Adding the machine-type filter to discover_and_parse must not
    change the ND2 diffusion result for the existing single-type
    scenario (dumper_truck, the only active type today).

    Scenario:
      - A SYSTEM channel 'BAT_LOW' declared in dumper_truck/combus/ with
        requires=[IS_MAINBOARD] on mainboard only.
      - soundboard sees the synthetic downlink (ND2 rule 5).

    We verify that:
      - With filter (machine_type='dumper_truck'): diffusion works and
        the synthetic copy is present on soundboard.
      - Without machine type (None): no machine type profile is loaded
        (link rework 3.1), so BAT_LOW is absent on both boards.
    """
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        project_root = tmp / "proj"
        src_dir = project_root / "src"
        src_dir.mkdir(parents=True)
        # Only dumper_truck/ is present (excavator/ is absent). The
        # filter is therefore a no-op for THIS test, which is exactly
        # what we want: it pins that the existing behaviour is
        # preserved.
        dumper_dir = src_dir / "core" / "config" / "machines" / "dumper_truck" / "combus"
        dumper_dir.mkdir(parents=True)
        _write_cb(dumper_dir / "mainboard.cb", """\
            channels:
              - id: BAT_LOW
                infoName: "Battery low"
                type: digital
                scope: SYSTEM
                theme: vbat
                direction: uplink
                requires:
                  - IS_MAINBOARD
            """)

        # --- Build the diffs: with and without the filter ------------
        results = {}
        for mt_label, mt_value in [("no_filter", None),
                                    ("filter_dumper_truck", "dumper_truck")]:
            ctx_main = _ctx_with({"IS_MAINBOARD"},
                                 src=src_dir)
            ctx_sound = _ctx_with({"IS_EXT_BOARD"},
                                  src=src_dir)
            node = NodeContext(
                mother_env="volvo_A60H_bruder",
                boards={"mainboard": ctx_main, "soundboard": ctx_sound},
                is_node=True,
            )
            # Discover + parse with the chosen filter.
            _br, _files, parsed = discover_and_parse(
                project_root=project_root, machine_type=mt_value,
            )
            results[mt_label] = diffuse_system_channels(parsed, node)

        # Without machine type: no machine type profile, BAT_LOW absent.
        for board_id in ("mainboard", "soundboard"):
            assert "BAT_LOW" not in _decls_by_id(results["no_filter"][board_id])

        # With the filter: BAT_LOW on mainboard is uplink, on soundboard
        # is downlink (ND2 rule 5).
        main_with = _decls_by_id(results["filter_dumper_truck"]["mainboard"])
        snd_with = _decls_by_id(results["filter_dumper_truck"]["soundboard"])
        assert main_with.get("BAT_LOW") == "uplink", (
            f"mainboard BAT_LOW expected uplink, got {main_with.get('BAT_LOW')!r}"
        )
        assert snd_with.get("BAT_LOW") == "downlink", (
            f"soundboard BAT_LOW expected downlink (ND2 rule 5), "
            f"got {snd_with.get('BAT_LOW')!r}"
        )


def _decls_by_id(parsed):
    """Extract {channel_id: direction} from a parsed list."""
    out = {}
    for _path, _type, raw in parsed:
        if not isinstance(raw, dict):
            continue
        section = raw.get("channels")
        if not isinstance(section, list):
            continue
        for decl in section:
            if isinstance(decl, dict):
                cid = decl.get("id")
                direction = decl.get("direction", "none")
                if isinstance(cid, str):
                    out[cid] = direction
    return out


# =============================================================================
# 6. Simulated different-type scenario (ND8 spec test #1)
# =============================================================================

def test_simulated_excavator_build_excludes_dumper_truck(tmp_path):
    """Spec test: a simulated MACHINE_TYPE_EXCAVATOR build must exclude
    the dumper_truck/ .cb files (and vice-versa).

    This is the third test from the spec:
      '`.cb` sous machines/dumper_truck/ inclus pour un build
       MACHINE_TYPE_DUMPER_TRUCK, exclu pour un type différent simulé
       (`.cb` fictif sous machines/excavator/).'

    We test the symmetric direction: a fake 'excavator' build
    excludes a fake 'dumper_truck/' .cb (the real dumper_truck/ .cb
    is irrelevant — we are checking the FILTER behaviour, not the
    real files).
    """
    src = tmp_path / "src"
    (src / "core" / "config" / "machines" / "dumper_truck" / "combus").mkdir(parents=True)
    (src / "core" / "config" / "machines" / "excavator" / "combus").mkdir(parents=True)
    _write_cb(src / "core" / "config" / "machines" / "dumper_truck" / "combus" / "dt.cb",
              "channels: []\n")
    _write_cb(src / "core" / "config" / "machines" / "excavator" / "combus" / "ex.cb",
              "channels: []\n")

    # Simulated MACHINE_TYPE_DUMPER_TRUCK build: keep dt.cb, exclude ex.cb.
    found = discover_config_files(src, machine_type="dumper_truck")
    found_strs = [str(p) for p in found]
    assert any("dt.cb" in s for s in found_strs)
    assert not any("ex.cb" in s for s in found_strs)

    # Simulated MACHINE_TYPE_EXCAVATOR build: keep ex.cb, exclude dt.cb.
    found = discover_config_files(src, machine_type="excavator")
    found_strs = [str(p) for p in found]
    assert any("ex.cb" in s for s in found_strs)
    assert not any("dt.cb" in s for s in found_strs)


# =============================================================================
# Main entry
# =============================================================================

def _run_all():
    """Run every test_* function in this module. Pytest is preferred when
    available; we fall back to a manual runner for the standalone case
    (the same pattern as the other tests in this directory)."""
    import inspect
    import traceback
    this = sys.modules[__name__]
    tests = [
        (name, fn)
        for name, fn in inspect.getmembers(this, inspect.isfunction)
        if name.startswith("test_")
    ]
    failures = []
    for name, fn in tests:
        try:
            with tempfile.TemporaryDirectory() as td:
                fn(Path(td))
        except Exception:
            failures.append((name, traceback.format_exc()))
            print(f"  FAIL {name}")
            continue
        print(f"  OK   {name}")
    print(f"\n{len(tests) - len(failures)}/{len(tests)} tests passed")
    if failures:
        for name, tb in failures:
            print(f"\n--- {name} ---\n{tb}")
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    # Try pytest first (the repo's standard test runner), fall back to
    # the manual runner.
    try:
        import pytest as _pytest
        sys.exit(_pytest.main([__file__, "-v"]))
    except ImportError:
        _run_all()
