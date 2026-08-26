#!/usr/bin/env python3
"""
Tests for coherence.py (A8) - CPPDEFINES / PlatformIO coherence check.

Coverage:
  - list_known_envs() parses [env:NAME] sections from platformio.ini
  - is_known_env() accepts / rejects the right names
  - unknown env raises UnknownEnvironmentError
  - check_coherence() reuses A4 (no re-implementation)
  - coherent env (override matches idedata shape) -> coherent=True
  - divergent env -> coherent=False + divergence fields populated
  - flag present in override -> present in report's "extra" set when idedata lacks it
  - flag absent in override -> present in report's "missing" set when idedata has it
  - flag with value -> correctly compared (matched AND mismatched)
  - skipped path: A4 acquire fails -> report.skipped=True
  - skipped path: idedata subprocess fails -> report.skipped=True
  - multi-env: each env produces an independent report
  - structural check: A8 imports from A4 (does not re-implement)
"""

from __future__ import annotations

import json
import subprocess
import sys
import textwrap
from pathlib import Path
from unittest.mock import patch

THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.combus_builder.coherence import (
    CoherenceError,
    CoherenceReport,
    UnknownEnvironmentError,
    check_coherence,
    check_coherence_multi,
    is_known_env,
    list_known_envs,
)
from scripts.combus_builder.flags import (
    BuildContext,
    BuildContextError,
    acquire_build_context,
    compare_context_to_idedata,
    read_idedata,
)


# =============================================================================
# platformio.ini parsing
# =============================================================================

def _write_platformio_ini(tmp_path: Path, body: str) -> Path:
    """Write a synthetic platformio.ini to tmp_path and return the project dir."""
    ini = tmp_path / "platformio.ini"
    ini.write_text(textwrap.dedent(body), encoding="utf-8")
    # platformio.ini parsing only requires the file; a fake `src/` is fine.
    (tmp_path / "src").mkdir(exist_ok=True)
    return tmp_path


def test_list_known_envs_finds_env_sections(tmp_path):
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:machines]
        build_flags = -DFOO

        [env:volvo_A60H_bruder]
        extends = env:machines

        [env:remotes]
        build_flags = -DBAR

        [env]
        ; SCons-only section - NOT an env.
        foo = bar
        """)
    envs = list_known_envs(tmp_path)
    assert envs == sorted(["machines", "volvo_A60H_bruder", "remotes"])


def test_list_known_envs_skips_non_env_sections(tmp_path):
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:only_env]
        foo = bar
        """)
    assert list_known_envs(tmp_path) == ["only_env"]


def test_list_known_envs_raises_if_no_platformio_ini(tmp_path):
    try:
        list_known_envs(tmp_path)
    except FileNotFoundError as e:
        assert "platformio.ini" in str(e)
        return
    raise AssertionError("FileNotFoundError expected")


def test_is_known_env_true(tmp_path):
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar

        [env:sound_node_volvo]
        foo = bar
        """)
    assert is_known_env("test_combus_loopback", tmp_path) is True
    assert is_known_env("sound_node_volvo", tmp_path) is True


def test_is_known_env_false(tmp_path):
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    assert is_known_env("does_not_exist", tmp_path) is False


# =============================================================================
# Structural / reuse-of-A4 check
# =============================================================================

def test_a8_imports_from_a4_not_reimplements():
    """A8 must reuse A4's API, not re-implement CPPDEFINES acquisition."""
    import scripts.combus_builder.coherence as a8
    # A8 exposes the same primitive helpers A4 uses.
    assert a8.acquire_build_context is acquire_build_context
    assert a8.read_idedata is read_idedata
    assert a8.compare_context_to_idedata is compare_context_to_idedata


# =============================================================================
# check_coherence -- happy path
# =============================================================================

def _fake_idedata_payload(defines: list[str]) -> dict:
    """Build a minimal idedata JSON dict matching the shape PlatformIO emits."""
    return {
        "defines": defines,
        "cc_path": "/usr/bin/gcc",
        "includes": [],
    }


def test_coherent_env_returns_coherent_report(tmp_path):
    """override and idedata agree -> coherent=True, no missing/extra."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:volvo_A60H_bruder]
        build_flags = -DFOO -DBAR=42
        """)
    override = ["-DFOO", "-DBAR=42"]
    idedata = _fake_idedata_payload(["FOO", "BAR=42"])

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        report = check_coherence(
            "volvo_A60H_bruder",
            project_dir=tmp_path,
            override_cppdefines=override,
            strict=True,
        )

    assert isinstance(report, CoherenceReport)
    assert report.coherent is True
    assert report.skipped is False
    assert report.env_name == "volvo_A60H_bruder"
    assert report.ctx_source == "override"
    assert report.missing_from_ctx == []
    assert report.extra_in_ctx == []
    assert report.value_mismatches == {}
    # Counts are propagated.
    assert report.ctx_defines_count == 1   # FOO
    assert report.ctx_values_count == 1    # BAR=42
    assert report.idedata_defines_count == 1
    assert report.idedata_values_count == 1


# =============================================================================
# check_coherence -- flag presence / absence
# =============================================================================

def test_flag_present_in_ctx_only(tmp_path):
    """A flag present in ctx but absent in idedata -> reported as 'extra_in_ctx'."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    override = ["-DEXTRA_FLAG", "-DFOO"]
    idedata = _fake_idedata_payload(["FOO"])  # only FOO in idedata

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=override,
            strict=False,
        )

    assert report.coherent is False
    assert "EXTRA_FLAG" in report.extra_in_ctx
    assert report.missing_from_ctx == []


def test_flag_absent_from_ctx_but_in_idedata(tmp_path):
    """A flag in idedata but absent in ctx -> reported as 'missing_from_ctx'."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    override = ["-DFOO"]
    idedata = _fake_idedata_payload(["FOO", "MISSING_FLAG"])

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=override,
            strict=False,
        )

    assert report.coherent is False
    assert "MISSING_FLAG" in report.missing_from_ctx
    assert report.extra_in_ctx == []


# =============================================================================
# check_coherence -- flags with values
# =============================================================================

def test_flag_with_matching_value(tmp_path):
    """FOO=1 in both ctx and idedata -> coherent."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    override = ["-DFOO=1"]
    idedata = _fake_idedata_payload(["FOO=1"])

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=override,
        )

    assert report.coherent is True
    assert report.value_mismatches == {}


def test_flag_with_mismatched_value(tmp_path):
    """FOO=1 in ctx but FOO=2 in idedata -> reported as value mismatch."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    override = ["-DFOO=1"]
    idedata = _fake_idedata_payload(["FOO=2"])

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=override,
            strict=False,
        )

    assert report.coherent is False
    assert "FOO" in report.value_mismatches
    assert report.value_mismatches["FOO"]["ctx"] == "1"
    assert report.value_mismatches["FOO"]["idedata"] == "2"


# =============================================================================
# check_coherence -- unknown env / skipped paths
# =============================================================================

def test_unknown_env_raises(tmp_path):
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:only_env]
        foo = bar
        """)
    try:
        check_coherence(
            "does_not_exist",
            project_dir=tmp_path,
            override_cppdefines=["-DFOO"],
        )
    except UnknownEnvironmentError as e:
        assert "does_not_exist" in str(e)
        assert "known envs" in str(e).lower() or "only_env" in str(e)
        return
    raise AssertionError("UnknownEnvironmentError expected")


def test_acquire_failure_marks_skipped(tmp_path):
    """When A4 cannot acquire the ctx, A8 returns a skipped report."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    with patch("scripts.combus_builder.coherence.acquire_build_context",
               side_effect=BuildContextError("simulated A4 failure")):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=["-DFOO"],
            strict=False,
        )

    assert report.skipped is True
    assert report.coherent is False
    assert "A4 could not acquire" in report.skip_reason
    assert "simulated A4 failure" in report.skip_reason


def test_idedata_failure_marks_skipped(tmp_path):
    """When idedata subprocess fails, A8 returns a skipped report."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    with patch("scripts.combus_builder.coherence.read_idedata",
               side_effect=BuildContextError("simulated idedata failure")):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=["-DFOO"],
            strict=False,
        )

    assert report.skipped is True
    assert "idedata" in report.skip_reason.lower()
    assert "simulated idedata failure" in report.skip_reason


def test_strict_raises_on_divergence(tmp_path):
    """strict=True + divergent -> raises CoherenceError."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    override = ["-DFOO"]
    idedata = _fake_idedata_payload(["FOO", "MISSING_FLAG"])

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        try:
            check_coherence(
                "test_combus_loopback",
                project_dir=tmp_path,
                override_cppdefines=override,
                strict=True,
            )
        except CoherenceError as e:
            assert "DIVERGENT" in str(e)
            assert "test_combus_loopback" in str(e)
            return
    raise AssertionError("CoherenceError expected")


def test_non_strict_returns_report_on_divergence(tmp_path):
    """strict=False + divergent -> returns report, does not raise."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:test_combus_loopback]
        foo = bar
        """)
    override = ["-DFOO"]
    idedata = _fake_idedata_payload(["FOO", "MISSING_FLAG"])

    with patch("scripts.combus_builder.coherence.read_idedata",
               return_value=idedata):
        report = check_coherence(
            "test_combus_loopback",
            project_dir=tmp_path,
            override_cppdefines=override,
            strict=False,
        )

    assert report.coherent is False
    assert report.skipped is False


# =============================================================================
# check_coherence_multi -- independent reports per env
# =============================================================================

def test_multi_env_each_tested_independently(tmp_path):
    """Two envs, different overrides -> independent reports."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:volvo_A60H_bruder]
        build_flags = -DFOO

        [env:remotes]
        build_flags = -DBAR
        """)
    # Build a "per-env" idedata dispatcher:
    def fake_read_idedata(project_dir, env_name, **kw):
        if env_name == "volvo_A60H_bruder":
            return _fake_idedata_payload(["FOO"])
        if env_name == "remotes":
            return _fake_idedata_payload(["BAR"])
        raise BuildContextError(f"unexpected env {env_name}")

    with patch("scripts.combus_builder.coherence.read_idedata",
               side_effect=fake_read_idedata):
        reports = check_coherence_multi(
            ["volvo_A60H_bruder", "remotes"],
            project_dir=tmp_path,
            override_cppdefines_per_env={
                "volvo_A60H_bruder": ["-DFOO"],
                "remotes": ["-DBAR"],
            },
            strict=True,
        )

    assert len(reports) == 2
    assert reports[0].env_name == "volvo_A60H_bruder"
    assert reports[0].coherent is True
    assert reports[1].env_name == "remotes"
    assert reports[1].coherent is True


def test_multi_env_one_divergent_one_coherent(tmp_path):
    """First env divergent -> CoherenceError raised, but reports built."""
    _write_platformio_ini(tmp_path, """\
        [platformio]
        src_dir = src

        [env:volvo_A60H_bruder]
        build_flags = -DFOO

        [env:remotes]
        build_flags = -DBAR
        """)
    def fake_read_idedata(project_dir, env_name, **kw):
        if env_name == "volvo_A60H_bruder":
            return _fake_idedata_payload(["FOO", "MISSING"])  # divergent
        if env_name == "remotes":
            return _fake_idedata_payload(["BAR"])             # coherent
        raise BuildContextError(f"unexpected env {env_name}")

    with patch("scripts.combus_builder.coherence.read_idedata",
               side_effect=fake_read_idedata):
        try:
            check_coherence_multi(
                ["volvo_A60H_bruder", "remotes"],
                project_dir=tmp_path,
                override_cppdefines_per_env={
                    "volvo_A60H_bruder": ["-DFOO"],
                    "remotes": ["-DBAR"],
                },
                strict=True,
            )
        except CoherenceError as e:
            assert "volvo_A60H_bruder" in str(e)
            assert "MISSING" in str(e)
            return
    raise AssertionError("CoherenceError expected")


# =============================================================================
# Real-platformio.ini discovery (project's own platformio.ini)
# =============================================================================

def test_list_known_envs_finds_real_project_envs():
    """Smoke check: the project's own platformio.ini exposes several envs."""
    envs = list_known_envs(REPO_ROOT)
    # We expect at least these (already verified manually during A8 design).
    expected_subset = {
        "machines", "volvo_A60H_bruder", "remotes",
        "sound_node_base", "sound_node_volvo", "test_combus_loopback",
    }
    assert expected_subset.issubset(set(envs)), (
        f"missing envs: {expected_subset - set(envs)}"
    )
